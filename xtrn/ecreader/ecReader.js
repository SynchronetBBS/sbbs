// ecReader
// A lightbar, threaded message reader for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com, 2012

js.branch_limit = 0;
console.clear();
console.putmsg("\1h\1wecReader by echicken, loading message threads...");

load("sbbsdefs.js");
load("msgutils.js");
load("frame.js");
load("tree.js");
load(js.exec_dir + "msglib.js");

var maxMessages = 0;			// How many messages to load per sub, 0 for all.
var threaded = true;			// False to default to flat view
var setPointers = true; 		// False to leave scan pointers unaffected by reading
var reverseThreads = true;		// True to list threads from most to least recently updated
var reverseMessages = false;	// True to list messages in a thread from newest to oldest
var lbg = BG_CYAN;				// Lightbar background
var lfg = WHITE;				// Foreground colour of highlighted text
var nfg = LIGHTGRAY;			// Foreground colour of non-highlighted text
var xfg = LIGHTCYAN;			// Subtree expansion indicator colour
var tfg = LIGHTCYAN;			// Uh, that line beside subtree items
var fbg = BG_BLUE;				// Title, Header, Help frame background colour
var urm = WHITE;				// Unread message foreground colour
var hfg = "\1h\1w"; 			// Heading text (CTRL-A format, for now)
var sffg = "\1h\1c";			// Heading sub-field text (CTRL-A format, for now)
var mfg = "\1n\1w";				// Message text colour (CTRL-A format, for now)

var messages, tree, frame, titleFrame, columnFrame, treeFrame, helpFrame,
	messageFrame, headerFrame, bodyFrame, messageBar, promptFrame,
	promptSubFrame;

var headerTextWidth = console.screen_columns - 9;
var msgNumberWidth = 9;
var dateWidth = 17;
var subjectWidth = Math.round(((console.screen_columns - 2) - (msgNumberWidth + dateWidth)) / 2);
var nameWidth = Math.round(subjectWidth / 2);
while(msgNumberWidth + dateWidth + subjectWidth + nameWidth > (console.screen_columns - 2))
	subjectWidth = subjectWidth - 1;

var formatItem = function(messageNumber, from, to, subject, wwt) {
	var retval = 
		format(
			"%-8s%-" + nameWidth + "s%-" + nameWidth + "s%-" + subjectWidth + "s%s",
			messageNumber,
			from.substr(0, nameWidth - 1),
			to.substr(0, nameWidth - 1),
			subject.substr(0, subjectWidth - 1),
			strftime("%m-%d-%Y %H:%I", wwt)
		);
	return retval;
}

var getFlatList = function(code) {
	var msgBase = new MsgBase(code);
	var header;
	var item;
	try {
		msgBase.open();
		var start = (maxMessages == 0) ? msgBase.first_msg : msgBase.last_msg - maxMessages;
		for(var m = start; m <= msgBase.last_msg; m++) {
			header = msgBase.get_msg_header(m);
			if(
				header === null || header.attr&MSG_DELETE
				||
				(!msgBase.hasOwnProperty('cfg') && header.to != user.alias && header.to != user.name && header.to_ext != user.number)
			) {
				continue;
			}
			messages.push(header);
		}
		msgBase.close();
		if(reverseThreads)
			messages.reverse();
		for(var m = 0; m < messages.length; m++) {
			item = formatItem(messages[m].number, messages[m].from, messages[m].to, messages[m].subject, messages[m].when_written_time);
			var i = tree.addItem(item, showMessage, messages[m], code);
			if(code != "mail" && messages[messages.length - 1].number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr)
				i.attr = urm;
			else if(code == "mail" && !(messages[messages.length - 1].attr&MSG_READ))
				i.attr = urm;
		}
	} catch(err) {
		log(LOG_DEBUG, err);
	}
	tree.open();
	tree.cycle();
}

var getThreadedList = function(code) {
	var threads = getMessageThreads(code);
	var msgBase = new MsgBase(code);
	var item;
	
	try {
		for(var t in ((!reverseThreads)?threads.thread:threads.order)) {
			var theThread = (reverseThreads)?threads.thread[threads.order[t]]:threads.thread[t];
			msgBase.open();
			var header = msgBase.get_msg_header(theThread.messages[0].number);
			msgBase.close();
			messages.push(header);
			item = formatItem(
				theThread.messages[0].number,
				theThread.messages[0].from,
				theThread.messages[0].to,
				theThread.messages[0].subject,
				theThread.messages[0].when_written_time
			);
			
			if(theThread.messages.length == 1)
				var i = tree.addItem(item, showMessage, messages[messages.length - 1], code);
			else
				var i = tree.addTree(item);
				
			if(msgBase.hasOwnProperty('cfg') && messages[messages.length - 1].number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr)
				i.attr = urm;
			else if(!msgBase.hasOwnProperty('cfg') && !(messages[messages.length - 1].attr&MSG_READ))
				i.attr = urm;
			
			if(theThread.messages.length == 1)
				continue;
			
			var mm = (reverseMessages) ? theThread.messages.length - 1 : 0;
			for(var m = 0; m < theThread.messages.length; m++) {
				msgBase.open();
				var header = msgBase.get_msg_header(theThread.messages[mm].number);
				msgBase.close();
				messages.push(header);
				item = formatItem(
					theThread.messages[mm].number,
					theThread.messages[mm].from,
					theThread.messages[mm].to,
					theThread.messages[mm].subject,
					theThread.messages[mm].when_written_time
				);
				var ii = i.addItem(item, showMessage, messages[messages.length - 1], code);
				if(!msgBase.hasOwnProperty('cfg') && messages[messages.length - 1].number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr) {
					ii.attr = urm;
					i.attr = urm;
				} else if(msgBase.hasOwnProperty('cfg') && messages[messages.length - 1].attr&MSG_READ == 0) {
					ii.attr = urm;
					i.attr = urm;
				}
				mm = (reverseMessages) ? mm - 1 : mm + 1;
			}
		}
		tree.open();
		tree.cycle();
	} catch(err) {
		log(LOG_DEBUG, err);
	}
}

var getList = function(code) {
	if(tree.status&1) {
		tree.close();
		treeFrame.clear();
	}
	tree = new Tree(treeFrame);
	tree.colors.lbg = lbg;
	tree.colors.lfg = lfg;
	tree.colors.fg = nfg;
	tree.colors.xfg = xfg;
	tree.colors.tfg = tfg;
	messages = [];
	var msgBase = new MsgBase(code);
	if(msgBase.hasOwnProperty('cfg')) {
		var mg = msgBase.cfg.grp_name;
		var ms = msgBase.cfg.name;
	} else {	
		var mg = "Private email";
		var ms = "Private email";
	}
	titleFrame.clear();
	titleFrame.putmsg(hfg + "Message Group: " + sffg + mg);
	titleFrame.crlf();
	titleFrame.putmsg(hfg + "    Sub Board: " + sffg + ms);
	(threaded) ? getThreadedList(code) : getFlatList(code);
}

var deleteMessage = function(header, code) {
	var ret = false;
	var msgBase = new MsgBase(code);
	msgBase.open();
	if(
		((msgBase.hasOwnProperty('cfg') && msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].is_operator)
		||
		(!msgBase.hasOwnProperty('cfg')))
		&&
		prompt("Delete message #" + header.number)
	)
		{
			msgBase.remove_msg(header.number);
		}
	msgBase.close();
	return;
}

var sendEmail = function() {
	var ret = true;
	console.putmsg(hfg + "Send email to: ");
	var to = console.getstr('', 64, K_LINE);
	if(to == "")
		ret = false;
	else if(system.matchuser(to) > 0)
		bbs.email(system.matchuser(to, WM_EMAIL));
	else if(netaddr_type(to) != NET_NONE)
		bbs.netmail(to, WM_NETMAIL)
	else
		ret = false;
	if(!ret) {
		console.putmsg(hfg + "Invalid user or netmail/email address.");
		console.crlf();
		console.pause();
	}
}

var prompt = function(str) {
	promptFrame.top();
	promptSubFrame.clear();
	frame.cycle();
	console.gotoxy(promptSubFrame.x, promptSubFrame.y + 1);
	var ret = console.yesno(str);
	promptFrame.bottom();
	frame.invalidate();
	return ret;
}

var showMessage = function(header, code) {
	var retval = "REFRESH";
	var userInput = false;
	var n = header.number;
	var currentMessage = messages.indexOf(header);

	messageFrame.open();
	messageFrame.top();
	var msgBase = new MsgBase(code);
	msgBase.open();
	if(setPointers && msgBase.hasOwnProperty('cfg') && header.number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr) {
		msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr = header.number;
	} else if(setPointers && !msgBase.hasOwnProperty('cfg')) {
		header.attr|=MSG_READ;
		msgBase.put_msg_header(header.number, header);
	}
	var body = msgBase.get_msg_body(header.number);
	msgBase.close();

	headerFrame.putmsg(
		format(
			hfg + "%14s " + sffg + header.subject.substr(0, headerTextWidth) + "\r\n"
			+ hfg + "%14s " + sffg + header.from.substr(0, headerTextWidth) + "\r\n"
			+ hfg + "%14s " + sffg + header.to.substr(0, headerTextWidth) + "\r\n"
			+ hfg + "%14s " + sffg + system.timestr(header.when_written_time),
			"Subject:", "From:", "To:", "Date:"
		)
	);
	bodyFrame.scrollTo(0, 0);
	bodyFrame.putmsg(mfg + word_wrap(body));
	bodyFrame.scrollTo(0, 0);

	while(userInput != "Q") {
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
		userInput = console.getkey().toUpperCase();
		switch(userInput) {
			case "R":
				var f = new File(system.node_dir + "QUOTES.TXT");
				f.open("w");
				f.write(body);
				f.close();
				frame.invalidate();
				console.clear();
				if(msgBase.hasOwnProperty('cfg'))
					bbs.post_msg(msgBase.cfg.code, WM_QUOTE, header);
				else if(!msgBase.hasOwnProperty('cfg') && header.from_net_type == NET_NONE)
					bbs.email(parseInt(header.from_ext), WM_EMAIL|WM_QUOTE, "", header.subject);
				else if(!msgBase.hasOwnProperty('cfg'))
					bbs.netmail(header.from_net_addr, WM_NETMAIL|WM_QUOTE, header.subject);
				frame.draw();
				retval = header;
				userInput = "Q";
				getList((msgBase.hasOwnProperty('cfg'))?msgBase.cfg.code:"mail");
				break;
			case "P":
				if(!threaded && currentMessage < (messages.length - 1)) {
					currentMessage++;
					h = messages[currentMessage];
					retval = h;
					userInput = "Q";
				} else if(threaded && currentMessage > 0) {
					currentMessage = currentMessage - 1;
					h = messages[currentMessage];
					retval = h;
					userInput = "Q";
				}
				break;
			case "N":
				if(!threaded && currentMessage > 0) {
					currentMessage = currentMessage - 1;
					h = messages[currentMessage];
					retval = h;
					userInput = "Q";
				} else if(threaded && currentMessage < (messages.length - 1)) {
					currentMessage++;
					h = messages[currentMessage];
					retval = h;
					userInput = "Q";
				}
				break;
			case KEY_UP:
				bodyFrame.scroll(0, -1);
				break;
			case KEY_DOWN:
				bodyFrame.scroll(0, 1);
				break;
			case KEY_HOME:
				bodyFrame.scrollTo(0, 0);
				break;
			case KEY_END:
				bodyFrame.end();
				bodyFrame.scrollTo(0, bodyFrame.data_height - bodyFrame.height);
				break;
			case "[":
				if(bodyFrame.offset.y - bodyFrame.height < 0)
					bodyFrame.scrollTo(0, 0);
				else
					bodyFrame.scroll(0, -bodyFrame.height);
				break;
			case "]":
				if(bodyFrame.offset.y + bodyFrame.height > bodyFrame.data_height)
					bodyFrame.scrollTo(bodyFrame.data_height - bodyFrame.height);
				else
					bodyFrame.scroll(0, bodyFrame.height);
				break;
			case KEY_DEL:
				if(deleteMessage(header, code)) {
					userInput = "Q";
					retval = "REFRESH";
				}
				break;
			default:
				break;
		}
	}
	messageFrame.clear();
	headerFrame.clear();
	bodyFrame.clear();
	if(retval == "REFRESH")
		messageFrame.close();
	return retval;
}

var init = function() {
	frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK|WHITE);
	titleFrame = new Frame(1, 1, console.screen_columns, 2, fbg|WHITE, frame);
	columnFrame = new Frame(1, 3, console.screen_columns, 1, fbg|WHITE, frame);
	treeFrame = new Frame(1, 4, console.screen_columns, console.screen_rows - 4, BG_BLACK|WHITE, frame);
	helpFrame = new Frame(1, console.screen_rows, console.screen_columns, 1, fbg|WHITE, frame);
	messageFrame = new Frame(1, 3, console.screen_columns, console.screen_rows - 3, BG_BLACK|WHITE, frame);
	headerFrame = new Frame(1, 3, console.screen_columns, 4, fbg|WHITE, messageFrame);
	bodyFrame = new Frame(1, 7, console.screen_columns, console.screen_rows - 7, BG_BLACK|WHITE, messageFrame);
	messageBar = new Frame(1, console.screen_rows, console.screen_columns, 1, fbg|WHITE, messageFrame);
	promptFrame = new Frame(20, Math.round(console.screen_rows / 2) - 3, console.screen_columns - 40, 6, fbg|WHITE, frame);
	promptSubFrame = new Frame(22, promptFrame.y + 1, promptFrame.width - 4, 4, BG_BLACK|WHITE, promptFrame);

	tree = new Tree(treeFrame);
	
	helpFrame.center(
		hfg + "UP" + sffg + "/" + hfg + "DN  "
		+ hfg + "[" + sffg + "PgUp/PgDn" + hfg + "]  "
		+ hfg + "HOME" + sffg + "/" + hfg + "END  "
		+ hfg + "T" + sffg + "hreaded  "
		+ hfg + "F" + sffg + "lat  "
		+ hfg + "A" + sffg + "rea  "
		+ hfg + "E" + sffg + "mail  "
		+ hfg + "P" + sffg + "ost  "
		+ hfg + "DEL" + sffg + "ete  "
		+ hfg + "Q" + sffg + "uit"
	);

	messageBar.center(
		hfg + "UP" + sffg + "/" + hfg + "DN  "
		+ hfg + "[" + sffg + "PgUp/PgDn" + hfg + "]  "
		+ hfg + "HOME" + sffg + "/" + hfg + "END  "
		+ hfg + "R" + sffg + "eply  "
		+ hfg + "P" + sffg + "revious  "
		+ hfg + "N" + sffg + "ext  "
		+ hfg + "DEL" + sffg + "ete  "
		+ hfg + "Q" + sffg + "uit"
	);

	columnFrame.putmsg(
		format(
			"%-9s%-" + nameWidth + "s%-" + nameWidth + "s%-" + subjectWidth + "s%s",
			"Msg #", "From", "To", "Subject", "Date"
		)
	);
	frame.open();
	messageFrame.bottom();
	headerFrame.bottom();
	promptFrame.bottom();
	getList(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code);
}

var cleanUp = function() {
	tree.close();
	frame.close();
	frame.delete();
	console.clear();
}

var main = function() {

	var userInput;
	var r;
	var cursub = msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code;

	while(userInput != "Q") {

		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);

		userInput = console.inkey(K_NONE, 5).toUpperCase();
		switch(userInput) {
			case "T":
				threaded = true;
				break;
			case "F":
				threaded = false;
				break;
			case "A":
				messageAreaSelector(4, 5, console.screen_columns - 6, console.screen_rows - 8, frame);
				cursub = msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code;
				break;
			case "P":
				frame.invalidate();
				console.clear();
				(cursub == "mail")?sendEmail():bbs.post_msg(cursub);
				frame.draw();
				break;
			case "E":
				cursub = "mail";
				break;
			case KEY_DEL:
				if(!(tree.currentItem instanceof Tree) && deleteMessage(tree.currentItem.args[0], cursub)) {
					tree.currentTree.deleteItem();
					getList(cursub);
				}
				break;
			default:
				if(tree.current === undefined)
					break;
				r = tree.getcmd(userInput);
				if(r != "REFRESH" && !r.hasOwnProperty("number"))
					continue;
				while(r.hasOwnProperty("number")) {
					frame.cycle();
					r = showMessage(r, cursub);
				}
				break;
		}
		getList(cursub);
	}
}

init();
main();
cleanUp();
exit();