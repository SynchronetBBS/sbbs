// ecReader
// A lightbar, threaded message reader for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com, 2012

js.branch_limit = 0;
console.clear();
console.putmsg("\1h\1wecReader by echicken, loading message threads...");

load("sbbsdefs.js");
load("msgutils.js");
load(js.exec_dir + "msglib.js");
load("frame.js");
load("tree.js");

var maxMessages = 0;			// How many messages to load per sub, 0 for all.
var showMail = true;			// Allow access to the private 'mail' sub-board
var threaded = true;			// False to default to flat view
var setPointers = true; 		// False to leave scan pointers unaffected by reading
var reverseThreads = true;		// True to list threads from most to least recently updated
var reverseMessages = false;	// True to list messages in a thread from oldest to newest
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

var messages;
var tree;
var currentMessage = 0;
var mail = false;

if(argc > 0 && argv[0])
	var msgBase = new MsgBase(argv[0]);
else
	var msgBase = new MsgBase(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code);

var frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK|WHITE);
var titleFrame = new Frame(1, 1, console.screen_columns, 2, fbg|WHITE, frame);
var columnFrame = new Frame(1, 3, console.screen_columns, 1, fbg|WHITE, frame);
var treeFrame = new Frame(1, 4, console.screen_columns, console.screen_rows - 4, BG_BLACK|WHITE, frame);
var helpFrame = new Frame(1, console.screen_rows, console.screen_columns, 1, fbg|WHITE, frame);
var messageFrame = new Frame(1, 3, console.screen_columns, console.screen_rows - 3, BG_BLACK|WHITE, frame);
var headerFrame = new Frame(1, 3, console.screen_columns, 4, fbg|WHITE, messageFrame);
var bodyFrame = new Frame(1, 7, console.screen_columns, console.screen_rows - 7, BG_BLACK|WHITE, messageFrame);
var messageBar = new Frame(1, console.screen_rows, console.screen_columns, 1, fbg|WHITE, messageFrame);
var promptFrame = new Frame(20, Math.round(console.screen_rows / 2) - 3, console.screen_columns - 40, 6, fbg|WHITE, frame);
var promptSubFrame = new Frame(22, promptFrame.y + 1, promptFrame.width - 4, 4, BG_BLACK|WHITE, promptFrame);

frame.open();
messageFrame.bottom();
headerFrame.bottom();
promptFrame.bottom();

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

var msgNumberWidth = 9;
var dateWidth = 17;
var subjectWidth = Math.round(((console.screen_columns - 2) - (msgNumberWidth + dateWidth)) / 2);
var nameWidth = Math.round(subjectWidth / 2);
while(msgNumberWidth + dateWidth + subjectWidth + nameWidth > (console.screen_columns - 2)) {
	subjectWidth = subjectWidth - 1;
}

columnFrame.putmsg(
	format(
		"%-9s%-" + nameWidth + "s%-" + nameWidth + "s%-" + subjectWidth + "s%s",
		"Msg #",
		"From",
		"To",
		"Subject",
		"Date"
	)
);
function formatItem(messageNumber, from, to, subject, wwt) {
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

function getFlatList() {
	var header = null;
	var item;
	if(!mail)
		var mb = msgBase;
	else
		var mb = new MsgBase('mail');
	try {
		mb.open();
		if(maxMessages == 0)
			var start = mb.first_msg;
		else
			var start = mb.last_msg - maxMessages;
		for(var m = start; m <= mb.last_msg; m++) {
			header = mb.get_msg_header(m);
			if(
				header === null
				||
				header.attr&MSG_DELETE
				||
				(mail
					&&
					header.to != user.alias
					&&
					header.to != user.name
					&&
					header.to_ext != user.number
				)
			)
				continue;
			messages.push(header);
		}
		mb.close();
		if(reverseThreads)
			messages.reverse();
		for(var m = 0; m < messages.length; m++) {
			item = formatItem(messages[m].number, messages[m].from, messages[m].to, messages[m].subject, messages[m].when_written_time);;
			var i = tree.addItem(item, showMessage, messages[m], mail);
			if(!mail && messages[messages.length - 1].number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr)
				i.attr = urm;
			else if(mail && !(messages[messages.length - 1].attr&MSG_READ))
				i.attr = urm;
		}
	} catch(err) {
		log(LOG_DEBUG, err);
	}
	tree.open();
	tree.cycle();
}

function getThreadedList() {
	if(!mail) {
		var threads = getMessageThreads(msgBase.cfg.code, maxMessages);
		var mb = msgBase;
	} else {
		var threads = getMessageThreads('mail', maxMessages);
		var mb = new MsgBase('mail');
	}
	var item;
	for(var t in ((!reverseThreads)?threads.thread:threads.order)) {
		if(!reverseThreads)
			var theThread = threads.thread[t];
		else
			var theThread = threads.thread[threads.order[t]];
		if(theThread.messages.length < 2) {
			mb.open();
			var header = mb.get_msg_header(theThread.messages[0].number);
			mb.close();
			messages.push(header);
			item = formatItem(
				theThread.messages[0].number,
				theThread.messages[0].from,
				theThread.messages[0].to,
				theThread.messages[0].subject,
				theThread.messages[0].when_written_time
			);
			var i = tree.addItem(item, showMessage, messages[messages.length - 1], mail);
			if(!mail && messages[messages.length - 1].number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr)
				i.attr = urm;
			else if(mail && !(messages[messages.length - 1].attr&MSG_READ))
				i.attr = urm;
			continue;
		}
		item = formatItem(
			theThread.messages[0].number,
			theThread.messages[0].from,
			theThread.messages[0].to,
			theThread.messages[0].subject,
			theThread.newest
		);
		st = tree.addTree(item);
		var mm = (reverseMessages) ? theThread.messages.length - 1 : 0;
		for(var m = 0; m < theThread.messages.length; m++) {
			mb.open();
			var header = mb.get_msg_header(theThread.messages[mm].number);
			mb.close();
			messages.push(header);
			item = formatItem(
				theThread.messages[mm].number,
				theThread.messages[mm].from,
				theThread.messages[mm].to,
				theThread.messages[mm].subject,
				theThread.messages[mm].when_written_time
			);
			var i = st.addItem(item, showMessage, messages[messages.length - 1], mail);
			if(!mail && messages[messages.length - 1].number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr) {
				i.attr = urm;
				st.attr = urm;
			} else if(mail && messages[messages.length - 1].attr&MSG_READ == 0) {
				i.attr = urm;
				st.attr = urm;
			}
			mm = (reverseMessages) ? mm - 1 : mm + 1;
		}
	}
	tree.open();
	tree.cycle();
}

function getList() {
	tree = new Tree(treeFrame);
	tree.colors.lbg = lbg;
	tree.colors.lfg = lfg;
	tree.colors.fg = nfg;
	tree.colors.xfg = xfg;
	tree.colors.tfg = tfg;
	messages = [];
	if(!mail) {
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
	if(threaded)
		getThreadedList();
	else
		getFlatList(false);
}

function showMessage(header, mail) {
	var retval = "REFRESH";
	var userInput = "";
	var h = null;
	var n = header.number;
	currentMessage = messages.indexOf(header);
	messageFrame.top();
	if(!mail) {
		mb = msgBase;
		if(setPointers && header.number > msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr)
			msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].scan_ptr = header.number;
		mb.open();
	} else {
		mb = new MsgBase('mail');
		mb.open();
		header.attr|=MSG_READ;
		mb.put_msg_header(header.number, header);
	}
	var body = mb.get_msg_body(header.number);
	mb.close();
	headerFrame.putmsg(
		hfg + format("%15s", "Subject: ") + sffg + header.subject.substr(0, console.screen_columns - 9)
		+ "\r\n" +
		hfg + format("%15s", "From: ") + sffg + header.from.substr(0, console.screen_columns - 9)
		+ "\r\n" +
		hfg + format("%15s", "To: ") + sffg + header.to.substr(0, console.screen_columns - 9)
		+ "\r\n" +
		hfg + format("%15s", "Date: ") + sffg + system.timestr(header.when_written_time)
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
				try {
					if(!mail)
						bbs.post_msg(mb.cfg.code, WM_QUOTE, header);
					else if(mail && header.from_net_type == NET_NONE)
						bbs.email(parseInt(header.from_ext), WM_EMAIL|WM_QUOTE, "", header.subject);
					else if(mail)
						bbs.netmail(header.from_net_addr, WM_NETMAIL|WM_QUOTE, header.subject);
				} catch(err) {
					log(LOG_ERR, err);
				}
				frame.draw();
				retval = header;
				userInput = "Q";
				getList();
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
				if(deleteMessage(header)) {
					userInput = "Q";
					retval = "REFRESH";
				} else {
					frame.cycle();
				}
				break;
			default:
				break;
		}
	}
	messageFrame.clear();
	headerFrame.clear();
	bodyFrame.clear();
	if(!retval.hasOwnProperty("number"))
		messageFrame.bottom();
	else if(mail)
		retval.mail = true;
	return retval;
}

function deleteMessage(header) {
	var ret = false;
	if(!mail && msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].is_operator) {
		if(prompt("Delete message #" + header.number)) {
			msgBase.open();
			msgBase.remove_msg(header.number);
			msgBase.close();
			ret = true;
		}
	} else if(mail && prompt("Delete this message")) {
		var mailBase = new MsgBase("mail");
		mailBase.open();
		mailBase.remove_msg(header.number);
		mailBase.close();
		ret = true;
	}
	return ret;
}

function sendEmail() {
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

function prompt(str) {
	promptFrame.top();
	promptSubFrame.clear();
	frame.cycle();
	console.gotoxy(promptSubFrame.x, promptSubFrame.y + 1);
	var ret = console.yesno(str);
	promptFrame.bottom();
	frame.invalidate();
	return ret;
}

getList();
var userInput = "";
var r = "";
while(userInput != "Q") {
	switch(userInput) {
		case "T":
			tree.close();
			treeFrame.clear();
			threaded = true;
			getList();
			break;
		case "F":
			tree.close();
			treeFrame.clear();
			threaded = false;
			getList();
			break;
		case "A":
			messageAreaSelector(4, 5, console.screen_columns - 6, console.screen_rows - 8, frame, showMail);
			msgBase = new MsgBase(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code);
			mail = false;
			getList();
			break;
		case "P":
			frame.invalidate();
			console.clear();
			if(!mail)
				bbs.post_msg(msgBase.cfg.code);
			else
				sendEmail();
			getList();
			frame.draw();
			break;
		case "E":
			mail = true;
			getList();
			break;
		case KEY_DEL:
			if(!(tree.currentItem instanceof Tree)) {
				if(deleteMessage(tree.currentItem.args[0])) {
					tree.currentTree.deleteItem();
					getList();
				}
			}
			break;
		default:
			if(tree.current === undefined)
				break;
			r = tree.getcmd(userInput);
			if(r == "REFRESH")
				getList();
			while(r.hasOwnProperty("number")) {
				frame.cycle();
				if(r.hasOwnProperty('mail') && r.mail)
					r = showMessage(r, true);
				else
					r = showMessage(r);
			}
			break;
	}
	userInput = console.inkey(K_NONE, 5).toUpperCase();
	if(frame.cycle())
		console.gotoxy(console.screen_columns, console.screen_rows);
}