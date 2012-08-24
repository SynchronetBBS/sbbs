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

var showMail = true;	// Allow access to the private 'mail' sub-board
var threaded = true;	// False to default to flat view
var lbg = BG_CYAN;		// Lightbar background
var lfg = WHITE;		// Foreground colour of highlighted text
var nfg = LIGHTGRAY;	// Foreground colour of non-highlighted text
var xfg = LIGHTCYAN;	// Subtree expansion indicator colour
var tfg = LIGHTCYAN;	// Uh, that line beside subtree items
var fbg = BG_BLUE;		// Title, Header, Help frame background colour
var hfg = "\1h\1w"; 	// Heading text (CTRL-A format, for now)
var sffg = "\1h\1c";	// Heading sub-field text (CTRL-A format, for now)
var mfg = "\1n\1w";		// Message text colour (CTRL-A format, for now)

var messages;
var tree;
var currentMessage = 0;
var mail = false;

if(argc > 0 && argv[0])
	var msgBase = new MsgBase(argv[0]);
else
	var msgBase = new MsgBase(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code);

var frame = new Frame(1, 1, 80, 24, BG_BLACK|WHITE);
var titleFrame = new Frame(1, 1, 80, 2, fbg|WHITE, frame);
var columnFrame = new Frame(1, 3, 80, 1, fbg|WHITE, frame);
var treeFrame = new Frame(1, 4, 80, 20, BG_BLACK|WHITE, frame);
var helpFrame = new Frame(1, 24, 80, 1, fbg|WHITE, frame);
var messageFrame = new Frame(1, 3, 80, 21, BG_BLACK|WHITE, frame);
var headerFrame = new Frame(1, 3, 80, 4, fbg|WHITE, messageFrame);
var bodyFrame = new Frame(1, 7, 80, 17, BG_BLACK|WHITE, messageFrame);
var messageBar = new Frame(1, 24, 80, 1, fbg|WHITE, messageFrame);

frame.open();
messageFrame.bottom();
headerFrame.bottom();

columnFrame.putmsg(
	format("%-9s", "Msg #")
	+ format("%-13s", "From")
	+ format("%-13s", "To")
	+ format("%-28s", "Subject")
	+ "Date"
);
helpFrame.putmsg(
	hfg + "HOME" + sffg + "/" + hfg + "END "
	+ hfg + "[" + sffg + "PgUp/PgDn" + hfg + "] "
	+ hfg + "N" + sffg + "ew Scan "
	+ hfg + "T" + sffg + "hreaded "
	+ hfg + "F" + sffg + "lat "
	+ hfg + "C" + sffg + "hange Area "
	+ hfg + "P" + sffg + "ost "
	+ hfg + "E" + sffg + "mail "
	+ hfg + "DEL" + sffg + "ete "
	+ hfg + "Q" + sffg + "uit"
);
messageBar.putmsg(
	hfg + "HOME" + sffg + "/" + hfg + "END "
	+ hfg + "[" + sffg + "PgUp/PgDn" + hfg + "] "
	+ hfg + "R" + sffg + "eply "
	+ hfg + "P" + sffg + "revious "
	+ hfg + "N" + sffg + "ext "
	+ hfg + "DEL" + sffg + "ete "
	+ hfg + "Q" + sffg + "uit"
);

function formatItem(messageNumber, from, to, subject, date) {
	var retval = 
			format("%-8s", messageNumber)
			+ format("%-13s", from.substr(0, 12))
			+ format("%-13s", to.substr(0, 12))
			+ format("%-28s", subject.substr(0, 27))
			+ strftime("%m-%d-%Y %H:%I", date);
	return retval;
}

function getFlatList(oldestFirst) {
	var header = null;
	var item;
	if(!mail)
		var mb = msgBase;
	else
		var mb = new MsgBase('mail');
	mb.open();
	for(var m = mb.first_msg; m <= mb.last_msg; m++) {
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
	if(oldestFirst === undefined || !oldestFirst)
		messages.reverse();
	for(var m = 0; m < messages.length; m++) {
		item = formatItem(messages[m].number, messages[m].from, messages[m].to, messages[m].subject, messages[m].when_written_time);;
		tree.addItem(item, showMessage, messages[m], mail);
	}
	tree.open();
	tree.cycle();
}

function getThreadedList() {
	if(!mail)
		var threads = getMessageThreads(msgBase.cfg.code);
	else
		var threads = getMessageThreads('mail');
	var item;
	for(var t in threads.order) {
		if(threads.thread[threads.order[t]].messages.length < 2) {
			messages.push(threads.thread[threads.order[t]].messages[0]);
			item = formatItem(
				threads.thread[threads.order[t]].messages[0].number,
				threads.thread[threads.order[t]].messages[0].from,
				threads.thread[threads.order[t]].messages[0].to,
				threads.thread[threads.order[t]].messages[0].subject,
				threads.thread[threads.order[t]].messages[0].when_written_time
			);
			tree.addItem(item, showMessage, messages[messages.length - 1], mail);
			continue;
		}
		item = formatItem(
			threads.thread[threads.order[t]].messages[0].number,
			threads.thread[threads.order[t]].messages[0].from,
			threads.thread[threads.order[t]].messages[0].to,
			threads.thread[threads.order[t]].messages[0].subject,
			threads.thread[threads.order[t]].newest
		);
		st = tree.addTree(item);
		for(var m = 0; m < threads.thread[threads.order[t]].messages.length; m++) {
			messages.push(threads.thread[threads.order[t]].messages[m]);
			item = formatItem(
				threads.thread[threads.order[t]].messages[m].number,
				threads.thread[threads.order[t]].messages[m].from,
				threads.thread[threads.order[t]].messages[m].to,
				threads.thread[threads.order[t]].messages[m].subject,
				threads.thread[threads.order[t]].messages[m].when_written_time
			);
			st.addItem(item, showMessage, messages[messages.length - 1], mail);
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

function showMessage(header) {
	currentMessage = messages.indexOf(header);
	messageFrame.top();
	if(!mail)
		mb = msgBase;
	else
		mb = new MsgBase('mail');
	mb.open();
	var body = mb.get_msg_body(header.number);
	mb.close();
	headerFrame.putmsg(
		hfg + format("%15s", "Subject: ") + sffg + header.subject.substr(0, 65)
		+ "\r\n" +
		hfg + format("%15s", "From: ") + sffg + header.from.substr(0, 65)
		+ "\r\n" +
		hfg + format("%15s", "To: ") + sffg + header.to.substr(0, 65)
		+ "\r\n" +
		hfg + format("%15s", "Date: ") + sffg + system.timestr(header.when_written_time)
	);
	bodyFrame.scrollTo(0, 0);
	bodyFrame.putmsg(mfg + word_wrap(body));
	bodyFrame.scrollTo(0, 0);
	var retval = true;
	var userInput = "";
	var h = null;
	var n = header.number;
	while(userInput != "Q") {
		if(frame.cycle())
			console.gotoxy(80, 24);
		userInput = console.getkey().toUpperCase();
		switch(userInput) {
			case "R":
				var f = new File(system.node_dir + "QUOTES.TXT");
				f.open("w");
				f.write(body);
				f.close();
				frame.invalidate();
				console.clear();
				if(!mail)
					bbs.post_msg(mb.cfg.code, WM_QUOTE, header);
				else if(mail && header.from_net_type == NET_NONE)
					bbs.email(parseInt(header.from_ext), WM_EMAIL|WM_QUOTE, "", "RE: " + header.subject);
				else if(mail)
					bbs.netmail(header.from_net_addr, WM_NETMAIL|WM_QUOTE, "RE: " + header.subject);
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
		case "C":
			messageAreaSelector(4, 5, 70, 16, frame);
			msgBase = new MsgBase(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].code);
			mail = false;
			getList();
			break;
		case "P":
			frame.invalidate();
			console.clear();
			bbs.post_msg(msgBase.cfg.code);
			getList();
			frame.draw();
		case "E":
			mail = true;
			getList();
			break;
		case KEY_DEL:
			break;
		default:
			r = tree.getcmd(userInput);
			while(r.hasOwnProperty("number")) {
				frame.cycle();
				if(r.hasOwnProperty('mail') && r.mail)
					r = showMessage(r, true);
				else
					r = showMessage(r);
			}
			break;
	}
	userInput = console.inkey(5).toUpperCase();
	if(frame.cycle())
		console.gotoxy(80, 24);
}