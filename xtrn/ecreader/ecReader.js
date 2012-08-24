// ecReader
// A lightbar, threaded message reader for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com, 2012

console.clear();
console.putmsg("\1h\1wecReader by echicken, loading message threads...");

load("sbbsdefs.js");
load("msgutils.js");
load(js.exec_dir + "msglib.js");
load("frame.js");
load("tree.js");

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
	hfg + "HOME"
	+ sffg + ", "
	+ hfg + "END"
	+ sffg + ", "
	+ hfg + "[" + sffg + " PgUp PgDn " + hfg + "] "
	+ sffg + "| View: "
	+ hfg + "T" + sffg + "hreaded, "
	+ hfg + "F" + sffg + "lat | "
	+ hfg + "C" + sffg + "hange area  "
	+ hfg + "P" + sffg + "ost  "
	+ hfg + "Q" + sffg + "uit"
);
messageBar.putmsg(
	sffg + "Scroll: " + hfg + "UP" + sffg + "/" + hfg + "DOWN"
	+ sffg + ", "
	+ hfg + "HOME"
	+ sffg + ", "
	+ hfg + "END"
	+ sffg + ", "
	+ hfg + "[" + sffg + " PgUp PgDn " + hfg + "] "
	+ sffg + " | "
	+ hfg + "R" + sffg + "eply  "
	+ hfg + "P" + sffg + "revious  "
	+ hfg + "N" + sffg + "ext  "
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
	msgBase.open();
	for(var m = msgBase.first_msg; m <= msgBase.last_msg; m++) {
		header = msgBase.get_msg_header(m);
		if(header === null || header.attr&MSG_DELETE)
			continue;
		messages.push(header);
	}
	msgBase.close();
	if(oldestFirst === undefined || !oldestFirst)
		messages.reverse();
	for(var m = 0; m < messages.length; m++) {
		tree.addItem(formatItem(messages[m].number, messages[m].from, messages[m].to, messages[m].subject, messages[m].when_written_time), showMessage, messages[m]);
	}
	tree.open();
	tree.cycle();
}

function getThreadedList() {
	var threads = getMessageThreads(msgBase.cfg.code);
	for(var t in threads.order) {
		if(threads.thread[threads.order[t]].messages.length < 2) {
			messages.push(threads.thread[threads.order[t]].messages[0]);
			tree.addItem(
					formatItem(
						threads.thread[threads.order[t]].messages[0].number,
						threads.thread[threads.order[t]].messages[0].from,
						threads.thread[threads.order[t]].messages[0].to,
						threads.thread[threads.order[t]].messages[0].subject,
						threads.thread[threads.order[t]].messages[0].when_written_time
					),
				showMessage, messages[messages.length - 1]
			);
			continue;
		}
		st = tree.addTree(
				formatItem(
					threads.thread[threads.order[t]].messages[0].number,
					threads.thread[threads.order[t]].messages[0].from,
					threads.thread[threads.order[t]].messages[0].to,
					threads.thread[threads.order[t]].messages[0].subject,
					threads.thread[threads.order[t]].newest
				)
		);
		for(var m = 0; m < threads.thread[threads.order[t]].messages.length; m++) {
			messages.push(threads.thread[threads.order[t]].messages[m]);
			st.addItem(
				formatItem(
					threads.thread[threads.order[t]].messages[m].number,
					threads.thread[threads.order[t]].messages[m].from,
					threads.thread[threads.order[t]].messages[m].to,
					threads.thread[threads.order[t]].messages[m].subject,
					threads.thread[threads.order[t]].messages[m].when_written_time
				),
				showMessage, messages[messages.length - 1]
			);
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
	titleFrame.clear();
	titleFrame.putmsg(hfg + "Message Group: " + sffg + msgBase.cfg.grp_name);
	titleFrame.crlf();
	titleFrame.putmsg(hfg + "    Sub Board: " + sffg + msgBase.cfg.name);
	if(threaded)
		getThreadedList();
	else
		getFlatList(false);
}

function showMessage(header) {
	currentMessage = messages.indexOf(header);
	messageFrame.top();
	msgBase.open();
	var body = msgBase.get_msg_body(header.number);
	msgBase.close();
	headerFrame.putmsg(
		hfg + format("%15s", "Subject: ") + sffg + header.subject
		+ "\r\n" +
		hfg + format("%15s", "From: ") + sffg + header.from
		+ "\r\n" +
		hfg + format("%15s", "To: ") + sffg + header.to
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
				bbs.post_msg(msgBase.cfg.code, WM_QUOTE, header);
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
			getList();
			break;
		case "P":
			frame.invalidate();
			console.clear();
			bbs.post_msg(msgBase.cfg.code);
			getList();
			frame.draw();
		default:
			r = tree.getcmd(userInput);
			while(r.hasOwnProperty("number")) {
				frame.cycle();
				r = showMessage(r);
			}
			break;
	}
	userInput = console.inkey(5).toUpperCase();
	if(frame.cycle())
		console.gotoxy(80, 24);
}