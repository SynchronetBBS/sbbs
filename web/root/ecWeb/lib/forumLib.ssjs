/* forumLib.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* A small library of functions that generate lists of boards, sub-boards and
   messages.  Much of the output style can be controlled via your theme's
   stylesheet. */

var sig = "";
var sigFile = user.number.toString();
while(sigFile.length < 4) sigFile = "0" + sigFile;
sigFile += ".sig";
if(file_exists(system.data_dir + "user/" + sigFile)) {
	var f = new File(system.data_dir + "user/" + sigFile);
	f.open("r",true);
	var sig = f.read().replace(/\n|\r\n/g, '&#13;&#10;');
	f.close();
}

function stripre(stripme) {
        stripme = stripme.toUpperCase().replace(/^\s*SPAM:/g, '');
        stripme = stripme.toUpperCase().replace(/^\s*RE:|^\s*RE/g, '');
        stripme = stripme.replace(/^\s*|\s*$/g, '');
        return stripme;
}

function sortnumber(a,b) {
        return b - a;
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
			return('<a href="'+link+'" class="linkified">'+linktext+'</a>'+str.substr(linktext.length));
		}
	);
	return(body);
}

function clean_subject(subject) {
	return html_encode(subject, true, true, false, false);
}

function webify(body) {
	var ret='';

	var blockquote_start='<div class="quote"><a class="quote-expander" href="#" onclick="toggle_quote(this.parentNode.parentNode);return false">Show quotes</a><blockquote style="display: none">';
	var blockquote_end='</blockquote></div>';

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
					var blockquote_start='<div class="quote"><small>&lt;--------------&gt;</small><blockquote style="display: none">';
					quote_depth++;
				}
				else if(broken) {
					line_prefix = line_prefix + blockquote_start;
					var blockquote_start='<div class="quote"><small>&lt;--------------&gt;</small><blockquote style="display: none">';
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
					var blockquote_start='<div class="quote"><small>&lt;--------------&gt;</small><blockquote style="display: none">';
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
	body=body.replace(/(\r?\n)/g, "<br>");
	body=body.replace(/'/g,'&rsquo;');
	return body;
}

// Generate a list of boards & their sub-boards, using client-side JS to toggle visibility of certain elements
function printBoards() {

	for(mg in msg_area.grp_list) {
		print("<div class='standardBorder standardPadding underMargin subBoardHeaderColor'>");
		print("<div class='headingFont'><a class=ulLink href='javascript:toggleVisibility(\"grp" + msg_area.grp_list[mg].number + "\")'>" + clean_subject(msg_area.grp_list[mg].name) + "</a></div>");
		print("<div id=stats" + msg_area.grp_list[mg].number + "></div>");
		print("</div>");
		print("<div id=grp" + msg_area.grp_list[mg].number + " style=display:none>");
		var a = 0;
		var b = 0;
		for(sb in msg_area.grp_list[mg].sub_list) {
			a++;
			var msgBase = new MsgBase(msg_area.grp_list[mg].sub_list[sb].code);
			msgBase.open();
			b = b + msgBase.total_msgs;
			print("<div class='standardBorder standardPadding underMargin treeIndent messageBoxColor'>");
			print('<a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + "&amp;action=viewSubBoard&amp;subBoard=" + clean_subject(msg_area.grp_list[mg].sub_list[sb].code) + '">' + clean_subject(msg_area.grp_list[mg].sub_list[sb].name) + "</a><br>");
			print(msgBase.total_msgs + " messages");
			if(msgBase.last_msg > 0) {
				var header = msgBase.get_msg_header(msgBase.last_msg);
				if(!header) {
					print("</div>");
					continue;
				}
				print("<br>Latest: " + clean_subject(header.subject) + ", by: " + clean_subject(header.from) + " on " + system.timestr(header.when_written_time));
			}
			print("</div>");
			msgBase.close();
		}
		print("</div>");
		print("<script type='text/javascript'>document.getElementById('stats" + msg_area.grp_list[mg].number + "').innerHTML = '" + clean_subject(msg_area.grp_list[mg].description) + "<br>" + b + " messages in " + a + " sub-boards';</script>");
	}
}

// Generate an index of all threads in a sub, with client side visibility toggles
/* This function is a total piece of crap IMHO, but it works (slowly.)
   Believe it or not, it's an improvement on the previous version. My
   goal at the moment is just to produce something functional and with
   acceptable performance. This function will need to be refined in many
   ways later on (printing formatted strings instead of assembling some
   step-by-step; remove conditions such as the 'thread_next' block if they
   prove to be unnecessary, etc.) This thing would probably be death on
   a slower computer. */
function printSubBoard(subBoardCode, threadNumber, newOnly, scanPointer, mg, sb) {

	var msgBase = new MsgBase(subBoardCode);
	if(!msgBase.open() || msgBase.last_msg < 1) {
		if(!newOnly) print("<br>There are no messages to show in this sub-board.");
		return;
	}
	var msgBase_last_msg=msgBase.last_msg;
	if(subBoardCode == 'mail') {
		var canPost=true;
	} else {
		var canPost=msgBase.cfg.can_post;
	}
	if(subBoardCode != 'mail' && !user.compare_ars(msgBase.cfg.ars)) return; // 'mail' does not have a .cfg.
	var header, body, msg, mm = msgBase.first_msg, reply = '', messageThreads = new Object(), threadedMessages = new Object();
	print('<div id=' + clean_subject(subBoardCode) + '-headerBox class="subBoardHeaderColor standardBorder standardMargin standardPadding headingFont">');

	if(subBoardCode == 'mail') {
		print('Private Mail');
	} else {
		print(msgBase.cfg.grp_name + ': ' + msgBase.cfg.name);
	}

	print('</div>');

	if(newOnly) {
		print('<script type="text/javascript">');
		print('document.getElementById("' + subBoardCode + '-headerBox").onclick = function() { toggleVisibility("subBoardContainer-' + subBoardCode + '"); updatePointer("' + subBoardCode + '", "' + eval(webIni.webUrl) + 'lib/forumAsync.ssjs", "' + mg + '", "' + sb + '"); };');
		print('</script>');
		print('<div id=subBoardContainer-' + clean_subject(subBoardCode) + ' style=display:none>');
	}

	if(subBoardCode == 'mail' || user.compare_ars(msgBase.cfg.post_ars)) {
		print('<div class="messageBoxColor standardBorder standardMargin standardPadding treeIndent">');

		if(webIni.maxMessages > 0 && !http_request.query.hasOwnProperty('showAll')) {
			print('Recent messages (<a class=ulLink href=' + eval(webIni.webUrl) + 'pages.ssjs?page=' + webIni.forumPage +'&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;showAll=true>Show all</a>)');
		} else {
			print('All messages');
			webIni.maxMessages = 0;
		}

		print(' - <a class=ulLink onclick=toggleVisibility(\'newMessage-' + clean_subject(subBoardCode) + '\')>Post a new message</a><br>');
		print('<div style=display:none; id=newMessage-' + clean_subject(subBoardCode) + '>');
		print('<br><form id=newMessageForm-' + clean_subject(subBoardCode) + ' action=none method=post>');
		print('To:<br><input type=text name=to size=50 value="All" onkeypress=noReturn(event)><br><br>');
		print('From:<br><select name=from><option value="' + user.alias + '">' + user.alias + '</option><option value="' + user.name + '">' + user.name + '</option></select><br><br>');
		print('Subject:<br><input type=text size=50 name=subject onkeypress=noReturn(event)><br><br>');
		print('<textarea class="standardBorder" name=body cols=80 rows=20>' + sig + '</textarea><br>');
		print('<input type=button value=Submit onclick=submitForm("newMessageForm-' + clean_subject(subBoardCode) + '","' + eval(webIni.webUrl) + 'lib/forumAsync.ssjs","newMessageForm-' + clean_subject(subBoardCode) + '")>');
		print('<input type=hidden name=subBoard value=' + clean_subject(subBoardCode) + '>');
		print('<input type=hidden name=irtMessage value=none>');
		print('</form></div></div>');
	}

	if(webIni.maxMessages > 0 && (msgBase_last_msg - webIni.maxMessages) > 0) mm = msgBase_last_msg - webIni.maxMessages;

	print('<script type="text/javascript">');
	print('function toggle_quote(container) {');
	print('	for(child in container.childNodes) {');
	print('		if(container.childNodes[child].nodeName=="BLOCKQUOTE") {');
	print('			switch(container.childNodes[child].style.display) {');
	print('			case "none":');
	print('				container.childNodes[child].style.display="block";');
	print('				toggle_quote(container.childNodes[child]);');
	print('				break;');
	print('			default:');
	print('				container.childNodes[child].style.display="none";');
	print('				toggle_quote(container.childNodes[child]);');
	print('				if(container.innerHTML=="Hide quotes")');
	print('					container.innerHTML="Show quotes";');
	print('				break;');
	print('			}');
	print('		}');
	print('		else if(container.childNodes[child].nodeName=="A") {');
	print('			if(container.childNodes[child].innerHTML=="Show quotes")');
	print('				container.childNodes[child].innerHTML="Hide quotes";');
	print('			else if(container.childNodes[child].innerHTML=="Hide quotes")');
	print('				container.childNodes[child].innerHTML="Show quotes";');
	print('		}');
	print('		else if(container.childNodes[child].nodeName=="SMALL") {');
	print('			if(container.childNodes[child].innerHTML=="&lt;--------------&gt;")');
	print('				container.childNodes[child].innerHTML="";');
	print('			else if(container.childNodes[child].innerHTML=="")');
	print('				container.childNodes[child].innerHTML="&lt;--------------&gt;";');
	print('		}');
	print('		else {');
	print('			toggle_quote(container.childNodes[child]);');
	print('		}');
	print('	}');
	print('}');
	print('</script>');

	for(m = mm; m <= msgBase_last_msg; m++) {
		header = msgBase.get_msg_header(m);
		if(!(header && (header.can_read===undefined || header.can_read)))
			continue;
		body = msgBase.get_msg_body(true,header.offset,header);
		if(!body || !header.hasOwnProperty("number") || threadedMessages.hasOwnProperty(header.number)) continue;
		if(newOnly && header.number <= scanPointer) continue; // This message precedes our scan pointer - don't waste any more time on it.
		if(subBoardCode == 'mail' && header.to != user.alias && header.to_ext != user.number && header.from != user.alias && header.from_ext != user.number) continue; // lol :|

		/* This whole msg/reply thing needs to be replaced by something
		   less crappy, but for now I'll just try to make the setup 
		   legible. */

		/* Set 'msg' to contain a formatted version of the current 
		   message 'm' which will later be appended to the appropriate
		   thread (or used to create a new one.) */
		msg = '<a name=' + header.number + '><\\/a>';
		msg += '<div id=' + clean_subject(subBoardCode) + header.number + ' class="messageBoxColor standardBorder standardPadding underMargin subTreeIndent messageFont">';
		msg += 'From <b>' + header.from + '<\\/b> to <b>' + header.to + '<\\/b> on <b>' + system.timestr(header.when_written_time) + '<\\/b>';
		msg += '<br><br>' + webify(body).replace(/<\//g,'<\\\/') + '<br>';

		if(subBoardCode == 'mail' || canPost) {
			msg += '<a class=ulLink onclick=toggleVisibility("' + clean_subject(subBoardCode) + '-reply-' + header.number + '")>Reply<\\/a> - ';
			/* Set 'reply' to contain a (non-submittable) reply form, which
			   will be appended to 'msg' (above) further along. The submit
			   button of this form is just a regular button that triggers
			   the submitForm() function from lib/clientLib.js which sends
			   the form data to the server via an XMLHttpRequest(). This 
			   way we don't have to migrate away from the page (bad.) */
			reply = '<div style=display:none;margin-top:10px; id=' + clean_subject(subBoardCode) + '-reply-' + header.number + '>';
			reply += '<form id=' + clean_subject(subBoardCode) + '-replyForm-' + header.number + ' action=none method=post>';
			reply += 'To:<br><input type=text name=to size=50 value="' + header.from + '" onkeypress=noReturn(event)><br><br>';
			reply += 'From:<br><select name=from><option value="' + user.alias + '">' + user.alias + '</option><option value="' + user.name + '">' + user.name + '</option></select><br><br>';
			reply += '<textarea class="standardBorder" name=body cols=80 rows=20>' + quote_msg(body, line_length=79, prefix="> ").replace(/\n|\r\n/g, "&#13;&#10;").replace(/'/g, '&rsquo;') + '&#13;&#10;' + sig + '</textarea><br>';
			reply += '<input type=button value=Submit onclick=submitForm("' + clean_subject(subBoardCode) + '-replyForm-' + header.number + '","' + eval(webIni.webUrl) + 'lib/forumAsync.ssjs","' + clean_subject(subBoardCode) + '-reply-' + header.number + '")>';
			reply += '<input type=hidden name=subject value="' + clean_subject(header.subject) + '">';
			reply += '<input type=hidden name=subBoard value=' + clean_subject(subBoardCode) + '>';
			reply += '<input type=hidden name=irtMessage value=' + header.number + '>';
			reply += '</form><br></div>';
		}

		if(header.thread_back > 0 && threadedMessages.hasOwnProperty(header.thread_back) && messageThreads.hasOwnProperty(threadedMessages[header.thread_back])) {
			// This message is in reply to another one which has already been threaded
			threadedMessages[header.number] = messageThreads[threadedMessages[header.thread_back]]['number'];
			messageThreads[threadedMessages[header.thread_back]]['newest'] = header.when_written_time;
			messageThreads[threadedMessages[header.thread_back]]['replies']++;
			messageThreads[threadedMessages[header.thread_back]]['latestAuthor'] = header.from;
			messageThreads[threadedMessages[header.thread_back]]['latestNumber'] = header.number;
			msg += '<a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + messageThreads[threadedMessages[header.thread_back]]["threadID"] + '">Thread URL<\\/a>';
			msg += ' - <a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + messageThreads[threadedMessages[header.thread_back]]["threadID"] + '#' + header.number + '">Message URL<\\/a>';
			msg += ' - <a class=ulLink onclick=toggleVisibility("threadContainer' + messageThreads[threadedMessages[header.thread_back]]["number"]+ '")>Collapse Thread<\\/a><br>' + reply + '<\\/div>';
			print("<script type='text/javascript'>");
			print("threadContainer" + messageThreads[threadedMessages[header.thread_back]]['number'] + ".innerHTML += '" + msg + "';");
			print("</script>");
		} else if(header.thread_next > 0 && threadedMessages.hasOwnProperty(header.thread_next) && messageThreads.hasOwnProperty(threadedMessages[header.thread_next])) {
			// A reply to this message has already been threaded (This condition may be unnecessary - test without later on)
			threadedMessages[header.number] = messageThreads[threadedMessages[header.thread_next]]['number'];
			messageThreads[threadedMessages[header.thread_next]]['newest'] = header.when_written_time;
			messageThreads[threadedMessages[header.thread_next]]['replies']++;
			messageThreads[threadedMessages[header.thread_next]]['latestAuthor'] = header.from;
			messageThreads[threadedMessages[header.thread_next]]['latestNumber'] = header.number;
			msg += '<a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + messageThreads[threadedMessages[header.thread_next]]["threadID"] + '">Thread URL<\\/a>';
			msg += ' - <a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + messageThreads[threadedMessages[header.thread_next]]["threadID"] + '#' + header.number + '">Message URL<\\/a>';
			msg += ' - <a class=ulLink onclick=toggleVisibility("threadContainer' + messageThreads[threadedMessages[header.thread_next]]["number"]+ '")>Collapse Thread<\\/a>' + reply + '<\\/div>';
			print("<script type='text/javascript'>");
			print("threadContainer" + messageThreads[threadedMessages[header.thread_next]]['number'] + ".innerHTML += '" + msg + "';");
			print("</script>");
		} else {
			// SMB threading data has been exhausted, so let's make sure there are no subject line matches in the existing threads before creating a new one

			for(var t in messageThreads) {

				if((stripre(messageThreads[t]["subject"]) == stripre(header.subject)) || (stripre(messageThreads[t]["subject"]).substr(0, stripre(header.subject).length) == stripre(header.subject))) {
					threadedMessages[header.number] = messageThreads[t]['number'];
					messageThreads[t]['newest'] = header.when_written_time;
					messageThreads[t]['replies']++;
					messageThreads[t]['latestAuthor'] = header.from;
					messageThreads[t]['latestNumber'] = header.number;
					msg += '<a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + messageThreads[t]["threadID"] + '">Thread URL<\\/a>';
					msg += ' - <a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + messageThreads[t]["threadID"] + '#' + header.number + '">Message URL<\\/a>';
					msg += ' - <a class=ulLink onclick=toggleVisibility("threadContainer' + messageThreads[t]["number"]+ '")>Collapse Thread<\\/a>' + reply + '<\\/div>';
					print("<script type='text/javascript'>");
					print("threadContainer" + messageThreads[t]['number'] + ".innerHTML += '" + msg + "';");
					print("</script>");
					break; // Need not waste time on any more message threads if a match was found.
				}

			}

		}

		if(!threadedMessages.hasOwnProperty(header.number)) {
			// This message is not associated with any existing threads based on the above criteria - time to create a new one
			messageThreads[threadNumber] = { 'number' : threadNumber, 'newest' : header.when_written_time, 'subject' : header.subject, 'replies' : 0, 'latestAuthor' : '', 'latestNumber' : header.number, 'threadID' : header.number };
			threadedMessages[header.number] = threadNumber;
			print("<script type='text/javascript'>");
			print("var threadHeader" + threadNumber + " = document.createElement('div');");
			print("threadHeader" + threadNumber + ".id = 'threadHeader" + threadNumber + "';");
			print("threadHeader" + threadNumber + ".innerHTML += '<a name=" + header.number + "><\\/a><a class=\"ulLink headingFont\" href=javascript:toggleVisibility(\"threadContainer" + threadNumber + "\")>" + clean_subject(header.subject, false, false, false, false).replace(/'/g, "&apos;") + "<\\/a><br>Started by " + header.from + " on " + system.timestr(parseInt(header.when_written_time)) + "';");
			print("var threadContainer" + threadNumber + " = document.createElement('div');");
			print("threadContainer" + threadNumber + ".id = 'threadContainer" + threadNumber + "';");

			if(http_request.query.hasOwnProperty('thread') && parseInt(http_request.query.thread) == header.number) {
				print("threadContainer" + threadNumber + ".style.display = 'block';");
			} else {
				print("threadContainer" + threadNumber + ".style.display = 'none';");
			}

			msg += '<a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + header.number + '">Thread URL<\\/a>';
			msg += ' - <a class=ulLink href="./pages.ssjs?page=' + webIni.forumPage + '&amp;action=viewSubBoard&amp;subBoard=' + clean_subject(subBoardCode) + '&amp;thread=' + header.number + '#' + header.number + '">Message URL<\\/a>';
			msg += ' - <a class=ulLink onclick=toggleVisibility("threadContainer' + threadNumber  + '")>Collapse Thread<\\/a>' + reply + '<\\/div>';
			print("threadContainer" + threadNumber + ".innerHTML += '" + msg + "';");
			print("</script>");
			threadNumber++;
		}

	}

	print("<div id=threadBox-" + clean_subject(subBoardCode) + "></div>");
	var newestDates = new Array();
	for(var t in messageThreads) newestDates.push(messageThreads[t]['newest']);
	newestDates = newestDates.sort(sortnumber);

	for(var d in newestDates) {

		for(var t in messageThreads) {
			if(messageThreads[t]['newest'] != newestDates[d]) continue;
			if(newOnly && messageThreads[t]['latestNumber'] <= scanPointer) continue;
			print("<script type='text/javascript'>");
			print("threadHeader" + t + ".className += 'messageBoxColor standardBorder standardPadding underMargin treeIndent';");

			if(messageThreads[t]['replies'] != 1) {
				print("threadHeader" + t + ".innerHTML += '<br>" + messageThreads[t]['replies'] + " replies';");
			} else {
				print("threadHeader" + t + ".innerHTML += '<br>" + messageThreads[t]['replies'] + " reply';");
			}

			if(messageThreads[t]['replies'] > 0) print("threadHeader" + t + ".innerHTML += ', latest by " + messageThreads[t]['latestAuthor'] + " on " + system.timestr(messageThreads[t]['newest']) + "';");
			print("document.getElementById('threadBox-" + subBoardCode + "').appendChild(threadHeader" + t + ");");
			print("document.getElementById('threadBox-" + subBoardCode + "').appendChild(threadContainer" + t + ");");
			print("</script>");
			break;
		}

	}
	
	if(newOnly) print("</div>"); // Close subBoardDiv-subBoardCode
	msgBase.close();
	return(threadNumber);
}

// Scan for new messages
/* This function is fairly rudimentary for now. It would probably benefit from
   some kind of paging or being limited to looking at the x most recent msgs.
   Any performance improvements to the above printSubBoards() function will
   benefit this function, which leans on printSubBoards() quite heavily. This
   function is *VERY* slow, and the more subs a user has set to scan and the
   more unread messages they have, the slower it gets. With a few thousand
   unread messages, it takes a couple of minutes to produce output. */
function newMessageScan() {
	var threadNumber = 0;
	for(mg in msg_area.grp_list) {
		for(sb in msg_area.grp_list[mg].sub_list) {
			if(msg_area.grp_list[mg].sub_list[sb].scan_cfg&SCAN_CFG_NEW) {
				var nsMsgBase = new MsgBase(msg_area.grp_list[mg].sub_list[sb].code);
				nsMsgBase.open();
				if(nsMsgBase.last_msg <= msg_area.grp_list[mg].sub_list[sb].scan_ptr) continue;
				nsMsgBase.close();
				threadShown = printSubBoard(msg_area.grp_list[mg].sub_list[sb].code, threadNumber, true, msg_area.grp_list[mg].sub_list[sb].scan_ptr, mg, sb);
			}
		}
	}
	if(threadNumber < 1) print("<br>No new messages.");
}
