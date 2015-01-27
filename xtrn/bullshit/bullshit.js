load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("scrollbar.js");
load("funclib.js");

js.branch_limit = 0;

var frame,
	titleFrame,
	listFrame,
	footerFrame,
	tree,
	ini,
	msgBase,
	treeScroll;

/*	drawFrameBorder(frame, color, gradient)
	Draws a border of color 'color' around Frame object 'frame'
	If 'gradient' is true, 'color' must be an array of colors to draw in order
	See sbbsdefs.js for valid colors	*/
var drawFrameBorder = function(frame, color) {
	var theColor = color;
	if(color instanceof Array)
		var sectionLength = Math.round(frame.width / color.length);
	for(var y = 1; y <= frame.height; y++) {
		for(var x = 1; x <= frame.width; x++) {
			if(x > 1 && x < frame.width && y > 1 && y < frame.height)
				continue;
			var msg = false;
			frame.gotoxy(x, y);
			if(y == 1 && x == 1)
				msg = ascii(218);
			else if(y == 1 && x == frame.width)
				msg = ascii(191);
			else if(y == frame.height && x == 1)
				msg = ascii(192);
			else if(y == frame.height && x == frame.width)
				msg = ascii(217);
			else if(x == 1 || x == frame.width)
				msg = ascii(179);
			else
				msg = ascii(196);
			if(color instanceof Array) {
				if(x == 1)
					theColor = color[0];
				else if(x % sectionLength == 0 && x < frame.width)
					theColor = color[x / sectionLength];
				else if(x == frame.width)
					theColor = color[color.length - 1];
			}
			frame.putmsg(msg, theColor);
		}
	}
}

var showMsg = function(msg) {
	try {
		var readerFrame = new Frame(2, 5, console.screen_columns - 2, console.screen_rows - 8, BG_BLACK|WHITE, frame);
		readerFrame.open();
		msgBase.open();
		var h = msgBase.get_msg_header(msg);
		var b = msgBase.get_msg_body(msg);
		msgBase.close();
		readerFrame.putmsg(
			format(
				"%-52s%s\r\n\r\n",
				h.subject.substr(0, console.screen_columns - 30),
				system.timestr(h.when_written_time)
			),
			getColor(ini.titleColor)
		);
		readerFrame.putmsg(word_wrap(b, console.screen_columns - 2), getColor(ini.textColor));
		readerFrame.scrollTo(0, 0);
		var scroller = new ScrollBar(readerFrame, { autohide : true });
		scroller.open();
		scroller.cycle();
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
	while(!js.terminated) {
		if(frame.cycle()) {
			scroller.cycle();
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
		var userInput = console.inkey(K_NONE, 5);
		if(userInput.toUpperCase() == "Q" || ascii(userInput) == 27)
			break;
		else if(userInput == KEY_UP && readerFrame.data_height > readerFrame.height)
			readerFrame.scroll(0, -1);
		else if(userInput == KEY_DOWN && readerFrame.data_height > readerFrame.height)
			readerFrame.scroll(0, 1);
	}
	readerFrame.close();
	readerFrame.delete();
	return true;
}

var init = function() {
	try {
		var f = new File(js.exec_dir + "bullshit.ini");
		f.open("r");
		ini = f.iniGetObject();
		f.close();
		ini.borderColor = ini.borderColor.toUpperCase().split(",");
		if(ini.borderColor.length > 1) {
			for(var b = 0; b < ini.borderColor.length; b++)
				ini.borderColor[b] = getColor(ini.borderColor[b]);
		} else {
			ini.borderColor = getColor(ini.borderColor[0]);
		}
		ini.lightbarForeground = getColor(ini.lightbarForeground);
		ini.lightbarBackground = getColor(ini.lightbarBackground);
	
		frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK|WHITE);
		titleFrame = new Frame(1, 1, console.screen_columns, 3, BG_BLACK|WHITE, frame);
		listFrame = new Frame(1, 4, console.screen_columns, console.screen_rows - 6, BG_BLACK|WHITE, frame);
		listTreeFrame = new Frame(2, 6, console.screen_columns - 2, console.screen_rows - 9, BG_BLACK|WHITE, listFrame);
		footerFrame = new Frame(1, console.screen_rows - 2, console.screen_columns, 3, BG_BLACK|WHITE, frame);
		drawFrameBorder(titleFrame, ini.borderColor);
		drawFrameBorder(listFrame, ini.borderColor);
		drawFrameBorder(footerFrame, ini.borderColor);
		tree = new Tree(listTreeFrame);
		tree.colors.lfg = ini.lightbarForeground;
		tree.colors.lbg = ini.lightbarBackground;

		var titleLen = console.screen_columns - 29;
		msgBase = new MsgBase(ini.messageBase);
		msgBase.open();
		for(var m = msgBase.last_msg; m >= msgBase.first_msg; m = m - 1) {
			try {
				var h = msgBase.get_msg_header(m);
				if(h === null)
					throw "Header is null";
				tree.addItem(
					format(
						"%-" + titleLen + "s%s",
						h.subject.substr(0, console.screen_columns - 29),
						system.timestr(h.when_written_time)
					),
					showMsg,
					m
				);
			} catch(err) {
				continue;
			}
		}
		msgBase.close();

		titleFrame.gotoxy(3, 2);
		titleFrame.putmsg("Bulletins", getColor(ini.titleColor));
		titleFrame.gotoxy(console.screen_columns - 25, 2);
		titleFrame.putmsg("bullshit v2 by echicken", DARKGRAY);
		listFrame.gotoxy(3, 2);
		listFrame.putmsg(format("%-" + titleLen + "s%s", "Title", "Date"), getColor(ini.headingColor));
		footerFrame.gotoxy(3, 2);
		footerFrame.putmsg("[ESC] or Q to quit, Up/Down arrows to scroll", getColor(ini.footerColor));
		treeScroll = new ScrollBar(tree);
		frame.open();
		tree.open();
	} catch(err) {
		log(LOG_ERR, err);
		return false;	
	}
	return true;
}

var main = function() {
	while(!js.terminated) {
		var userInput = console.inkey(K_NONE, 5);
		if(userInput.toUpperCase() == "Q" || ascii(userInput) == 27)
			break;
		tree.getcmd(userInput);
		if(frame.cycle()) {
			treeScroll.cycle();
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
	}
}

if(init()) {
	main();
	frame.close();
}
exit();
