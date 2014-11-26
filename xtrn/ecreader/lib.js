/*	ecReader reusable objects
	by echicken

	Chooser - Lightbar message group/area selector
	Lister - Lightbar flat/threaded message lister
	Reader - Scrollable message reader/browser

	You can use these objects on their own if you wish - see the examples.
	These have a lot of optional configurable values.  See the code. :|

	ecReader itself is a small script that cobbles all three together
	into an application.

	Examples:

	// Chooser
	load("sbbsdefs.js");
	load("../xtrn/ecreader/lib.js");
	var c = new Chooser();
	while(!js.terminated) {
		var i = console.inkey(K_NONE, 5);
		var r = c.getcmd(i);
		if(typeof r == "string")
			break;
		c.cycle();
	}
	c.close();

	// Lister
	load("sbbsdefs.js");
	load("../xtrn/ecreader/lib.js");
	var l = new Lister({ 'callback' : function(header) { log(JSON.stringify(header)) } });
	while(!js.terminated) {
		var i = console.inkey(K_NONE, 5);
		if(!l.getcmd(i))
			break;
		l.cycle();
	}
	l.close();

	// Reader
	load("sbbsdefs.js");
	load("../xtrn/ecreader/lib.js");
	var r = new Reader({ 'sub' : 'general', 'msg' : 1 });
	while(!js.terminated) {
		var i = console.inkey(K_NONE, 5);
		if(!r.getcmd(i))
			break;
		r.cycle();
	}
	r.close();
*/

load("sbbsdefs.js");
load("funclib.js");
load("frame.js");
load("tree.js");
load("msgutils.js");
load("scrollbar.js");

var Chooser = function(options) {

	if(typeof options == "undefined")
		options = {};

	var settings = {
		'sub' : (typeof options.sub != "string") ? bbs.cursub_code : options.sub,
		'grp' : (typeof options.grp != "number") ? bbs.curgrp : options.grp,
		'x' : (typeof options.x != "number") ? 1 : options.x,
		'y' : (typeof options.y != "number") ? 1 : options.y,
		'width' : (typeof options.width != "number") ? console.screen_columns : options.width,
		'height' : (typeof options.height != "number") ? console.screen_rows : options.height,
		'colors' : {
			'fhc' : (typeof options.fhc != "number") ? "\1h\1c" : getColor(options.fhc),
			'fvc' : (typeof options.fvc != "number") ? "\1h\1w" : getColor(options.fvc),
			'lbg' : (typeof options.lbg != "number") ? BG_CYAN : options.lbg,
			'lfg' : (typeof options.lfg != "number") ? WHITE : options.lfg,
			'nfg' : (typeof options.nfg != "number") ? DARKGRAY : options.nfg,
			'xfg' : (typeof options.xfg != "number") ? LIGHTCYAN : options.xfg,
			'tfg' : (typeof options.tfg != "number") ? LIGHTCYAN : options.tfg,
			'fbg' : (typeof options.fbg != "number") ? BG_BLUE : options.fbg
		}		
	};

	var frames = {}, tree, scroller;

	var initFrames = function() {

		frames.top = new Frame(
			settings.x,
			settings.y,
			settings.width,
			settings.height,
			LIGHTGRAY,
			options.frame
		);

		frames.header = new Frame(
			frames.top.x,
			frames.top.y,
			frames.top.width,
			1,
			settings.colors.fbg,
			frames.top
		);
		frames.header.putmsg(settings.colors.fvc + "Select a message group & area:");

		frames.footer = new Frame(
			frames.top.x,
			frames.top.y + frames.top.height - 1,
			frames.top.width,
			1,
			settings.colors.fbg,
			frames.top
		);
		frames.footer.putmsg(
			format(
				"%sCurrent group: %s%s, %sCurrent area: %s%s, %sQ%suit",
				settings.colors.fhc, settings.colors.fvc, msg_area.grp_list[settings.grp].name,
				settings.colors.fhc, settings.colors.fvc, msg_area.sub[settings.sub].name,
				settings.colors.fhc, settings.colors.fvc
			)
		);

		frames.list = new Frame(
			frames.top.x,
			frames.header.y + frames.header.height,
			frames.top.width,
			frames.top.height - frames.header.height - frames.footer.height,
			0,
			frames.top
		);

		if(typeof frames.top.parent == "undefined" || frames.top.parent.is_open)
			frames.top.open();

	}

	var setArea = function(code) {
		bbs.cursub_code = code;
		return code;
	}

	var buildTree = function() {
		tree = new Tree(frames.list);
		for(var color in settings.colors) {
			if(typeof tree.colors[color] !== "undefined")
				tree.colors[color] = settings.colors[color];
		}
		for(var g in msg_area.grp_list) {
			if(msg_area.grp_list[g].sub_list.length < 1)
				continue;
			var groupTree = tree.addTree(msg_area.grp_list[g].name);
			for(var s in msg_area.grp_list[g].sub_list) {
				groupTree.addItem(
					msg_area.grp_list[g].sub_list[s].name,
					setArea, msg_area.grp_list[g].sub_list[s].code
				);
			}
		}
		tree.open();
		scroller = new ScrollBar(tree, { autohide : true });
		scroller.open();
	}

	var init = function() {
		initFrames();
		buildTree();
	}

	this.getcmd = function(userInput) {
		if(userInput.toUpperCase() == "Q")
			return "QUIT";
		return tree.getcmd(userInput);
	}

	this.cycle = function() {
		if(typeof frames.top.parent != "undefined") {
			scroller.cycle();
		} else if(frames.top.cycle()) {
			scroller.cycle();
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
	}

	this.close = function() {
		frames.top.delete();
	}

	init();

}

var Lister = function(options) {

	if(typeof options == "undefined")
		options = {};

	var settings = {
		'sub' : (typeof options.sub != "string") ? bbs.cursub_code : options.sub,
		'x' : (typeof options.x != "number") ? 1 : options.x,
		'y' : (typeof options.y != "number") ? 1 : options.y,
		'width' : (typeof options.width != "number") ? console.screen_columns : options.width,
		'height' : (typeof options.height != "number") ? console.screen_rows : options.height,
		'max' : options.max,
		'callback' : (typeof options.callback == "undefined") ? function() {} : options.callback,
		'colors' : {
			'fhc' : (typeof options.fhc != "number") ? "\1h\1c" : getColor(options.fhc),
			'fvc' : (typeof options.fvc != "number") ? "\1h\1w" : getColor(options.fvc),
			'lbg' : (typeof options.lbg != "number") ? BG_CYAN : options.lbg,
			'lfg' : (typeof options.lfg != "number") ? WHITE : options.lfg,
			'nfg' : (typeof options.nfg != "number") ? DARKGRAY : options.nfg,
			'xfg' : (typeof options.xfg != "number") ? LIGHTCYAN : options.xfg,
			'tfg' : (typeof options.tfg != "number") ? LIGHTCYAN : options.tfg,
			'fbg' : (typeof options.fbg != "number") ? BG_BLUE : options.fbg,
			'urm' : (typeof options.urm != "number") ? WHITE : options.urm
		},
		'threaded' : (typeof options.threading != "boolean") ? true : false,
		'ascending' : (typeof options.ascending != "boolean") ? false : true,
		'threadAscending' : (typeof options.threadAscending != "boolean") ? true : false
	};

	var state = {
		'listing' : true,
		'choosing' : false
	};

	var frames = {}, tree, chooser, scroller;

	var threadFooter = format(
		"%sC%shange group/area, %sO%srder, %sT%shread order, %sF%slat view, %sP%sost, %s[%s Pg Up/Dn %s]%s, %sQ%suit",
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc
	);

	var flatFooter = format(
		"%sC%shange group/area, %sO%srder, %sT%shreaded view, %sP%sost, %s[%s Pg Up/Dn %s]%s, %sQ%suit",
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc,
		settings.colors.fhc, settings.colors.fvc
	);

	var setHeader = function() {
		frames.header.clear();
		frames.header.putmsg(
			format(
				"%sGroup: %s%s%s, Area: %s%s",
				settings.colors.fhc,
				settings.colors.fvc,
				msg_area.sub[settings.sub].grp_name,
				settings.colors.fhc,
				settings.colors.fvc,
				msg_area.sub[settings.sub].name
			)
		);
		frames.header.gotoxy(1, 2);
		frames.header.putmsg(
			formatListItem(
				{	'from' : " From",
					'to' : " To",
					'subject' : " Subject"
				}
			)
		);
	}

	var initFrames = function() {

		frames.top = new Frame(
			settings.x,
			settings.y,
			settings.width,
			settings.height,
			LIGHTGRAY,
			options.frame
		);

		frames.header = new Frame(
			frames.top.x,
			frames.top.y,
			frames.top.width,
			2,
			BG_BLUE|WHITE,
			frames.top
		);

		frames.footer = new Frame(
			frames.top.x,
			frames.top.y + frames.top.height - 1,
			frames.top.width,
			1,
			BG_BLUE|WHITE,
			frames.top
		);
		frames.footer.putmsg((settings.threaded) ? threadFooter : flatFooter);

		frames.list = new Frame(
			frames.top.x,
			frames.header.y + frames.header.height,
			frames.top.width,
			frames.top.height - frames.header.height - frames.footer.height,
			BG_BLUE|WHITE,
			frames.top
		);

		setHeader();

		if(typeof frames.top.parent == "undefined" || frames.top.parent.is_open)
			frames.top.open();

	}

	var formatListItem = function(header) {
		var nameWidth = Math.floor(frames.list.width / 5);
		var dateWidth = 14;
		var subjectWidth = frames.list.width - dateWidth - (nameWidth * 2) - 5;
		return format(
			"%-" + nameWidth + "s %-" + nameWidth + "s %-" + subjectWidth + "s %s",
			header.from.substr(0, nameWidth),
			header.to.substr(0, nameWidth),
			header.subject.substr(0, subjectWidth),
			((typeof header.when_written_time == "undefined") ? " Date" : strftime("%d/%m/%y %H:%I", header.when_written_time))
		);
	}

	var buildTree = function() {
		if(tree instanceof Tree)
			tree.close();
		tree = new Tree(frames.list);
		for(var color in settings.colors) {
			if(typeof tree.colors[color] !== "undefined")
				tree.colors[color] = settings.colors[color];
		}
		if(settings.threaded)
			buildThreadedTree();
		else
			buildFlatTree();
		scroller = new ScrollBar(tree);
		scroller.open();
		tree.open();
	}

	var buildFlatTree = function() {
		var mb = new MsgBase(settings.sub);
		mb.open();
		var headers = mb.get_all_msg_headers();
		mb.close();
		var keys = Object.keys(headers);
		if(!settings.ascending)
			keys.reverse();
		for(var k = 0; k < keys.length; k++) {
			var item = tree.addItem(
				formatListItem(headers[keys[k]]),
				options.callback,
				settings.sub,
				headers[keys[k]]
			);
			if(headers[keys[k]] >= msg_area.sub[settings.sub].scan_ptr)
				item.attr = settings.colors.urm;
		}
	}

	var buildThreadedTree = function() {
		var threads = getMessageThreads(settings.sub, settings.max);
		if(!settings.ascending)
			threads.order.reverse();
		for(var t in threads.order) {
			var threadTree = tree.addTree(
				formatListItem(threads.thread[threads.order[t]].messages[0])
			);
			if(!settings.threadAscending)
				threads.thread[threads.order[t]].messages.reverse();
			for(var m in threads.thread[threads.order[t]].messages) {
				var item = threadTree.addItem(
					formatListItem(threads.thread[threads.order[t]].messages[m]),
					options.callback,
					settings.sub,
					threads.thread[threads.order[t]].messages[m]
				);
				if(threads.thread[threads.order[t]].messages[m].number >= msg_area.sub[settings.sub].scan_ptr) {
					threadTree.attr = settings.colors.urm;
					item.attr = settings.colors.urm;
				}
			}
		}
	}

	var init = function() {
		initFrames();
		buildTree();
	}

	this.getcmd = function(userInput) {
		var ret = true;
		if(state.choosing) {
			chooser.cycle();
			var r = chooser.getcmd(userInput);
			if(typeof r == "string") {
				state.listing = true;
				state.choosing = false;
				chooser.close();
				settings.sub = bbs.cursub_code;
				setHeader();
				buildTree();
			}
		} else {
			switch(userInput.toUpperCase()) {
				case "Q":
					ret = false;
					break;
				case "C":
					state.listing = false;
					state.choosing = true;
					chooser = new Chooser(
						{	'sub' : settings.sub,
							'grp' : msg_area.sub[settings.sub].grp_index,
							'frame' : (typeof frames.top.parent == "undefined") ? frames.top : frames.top.parent,
							'colors' : settings.colors
						}
					);
					break;
				case "O":
					settings.ascending = (settings.ascending) ? false : true;
					buildTree();
					break;
				case "F":
					if(settings.threaded) {
						settings.threaded = false;
						buildTree();
						frames.footer.clear();
						frames.footer.putmsg(flatFooter);
					}
					break;
				case "T":
					if(settings.threaded) {
						settings.threadAscending = (settings.threadAscending) ? false : true;
					} else {
						settings.threaded = true;
						frames.footer.clear();
						frames.footer.putmsg(threadFooter);
					}
					buildTree();
					break;
				case "P":
					if(!msg_area.sub[settings.sub].can_post)
						break;
					console.clear(7);
					bbs.post_msg(settings.sub);
					if(typeof frames.top.parent != "undefined")
						frames.top.parent.invalidate();
					else
						frames.top.invalidate();
					break;
				default:
					tree.getcmd(userInput);
					break;
			}
		}
		return ret;
	}

	this.cycle = function() {
		if(typeof frames.top.parent != "undefined") {
			scroller.cycle();
		} else if(frames.top.cycle()) {
			scroller.cycle();
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
	}

	this.close = function() {
		frames.top.close();
		frames.top.delete();
	}

	init();

}

var Reader = function(options) {

	if(typeof options == "undefined")
		var options = {};

	var settings = {
		'sub' : (typeof options.sub != "string") ? bbs.cursub_code : options.sub,
		'x' : (typeof options.x != "number") ? 1 : options.x,
		'y' : (typeof options.y != "number") ? 1 : options.y,
		'width' : (typeof options.width != "number") ? console.screen_columns : options.width,
		'height' : (typeof options.height != "number") ? console.screen_rows : options.height,
		'fhc' : (typeof options.fhc != "number") ? "\1h\1c" : getColor(options.fhc),
		'fvc' : (typeof options.fvc != "number") ? "\1h\1w" : getColor(options.fvc),
		'thread' : (typeof options.thread != "boolean") ? true : false
	};
	settings.msg = (typeof options.msg == "undefined") ? msg_area.sub[settings.sub].scan_ptr : options.msg;

	var frames = {}, scroller;

	var state = {
		'header' : {},
		'body' : "",
		'thread_back_index' : {},
		'thread_next_index' : {},
		'thread_first_index' : {}
	};

	var initFrames = function() {

		frames.top = new Frame(
			settings.x,
			settings.y,
			settings.width,
			settings.height,
			LIGHTGRAY,
			options.frame
		);

		frames.header = new Frame(
			frames.top.x,
			frames.top.y,
			frames.top.width,
			4,
			BG_BLUE|WHITE,
			frames.top
		);

		frames.footer = new Frame(
			frames.top.x,
			frames.top.y + frames.top.height - 1,
			frames.top.width,
			1,
			BG_BLUE|WHITE,
			frames.top
		);
		// Herrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrp
		// Sometimes I do these things just to amuse myself.
		var footerString = format(
			"%sUp%s/%sDn%s scroll, %s[%sPg Up/Dn%s]%s, %sLEFT%s Browse %sRIGHT%s, %s< %sThread %s. >%s, %sR%seply, %sP%sost, %sQ%suit",
			settings.fhc, settings.fvc, settings.fhc, settings.fvc, // Up/Dn
			settings.fhc, settings.fvc, settings.fhc, settings.fvc, // [Page Up/Dn]
			settings.fhc, settings.fvc, settings.fhc, settings.fvc, // LEFT Browse RIGHT
			settings.fhc, settings.fvc, settings.fhc, settings.fvc, // < Thread . >
			settings.fhc, settings.fvc, // Reply
			settings.fhc, settings.fvc, // Post
			settings.fhc, settings.fvc // Quit
		);
		if(!settings.thread) {
			footerString = footerString.split(",");
			footerString.splice(3, 1);
			footerString = footerString.join();
		}
		frames.footer.putmsg(footerString);

		if(typeof frames.top.parent == "undefined" || frames.top.parent.is_open)
			frames.top.open();

	}

	var showMessage = function() {
		var mb = new MsgBase(settings.sub);
		mb.open();
		state.header = mb.get_msg_header(settings.msg);
		if(state.header !== null) {
			state.thread_back_index = mb.get_msg_index(state.header.thread_back);
			state.thread_next_index = mb.get_msg_index(state.header.thread_next);
			state.thread_first_index = mb.get_msg_index(state.header.thread_first);
		}
		state.body = mb.get_msg_body(settings.msg);
		var last = mb.last_msg;
		mb.close();
		if(state.header === null || state.body === null)
			return;
		if(state.header.number > msg_area.sub[settings.sub].scan_ptr)
			msg_area.sub[settings.sub].scan_ptr = state.header.number;
		frames.header.clear();
		frames.header.putmsg(
			format(
				"%s   From: %s%s\r\n%s     To: %s%s\r\n%s   Date: %s%s\r\n%sSubject: %s%s",
				settings.fhc, settings.fvc,	state.header.from,
				settings.fhc, settings.fvc,	state.header.to,
				settings.fhc, settings.fvc, system.timestr(state.header.when_written_time),
				settings.fhc, settings.fvc, state.header.subject
			)
		);
		var longestValue = Math.max(
			last.toString().length,
			msg_area.sub[settings.sub].grp_name.length,
			settings.sub.length
		);
		frames.header.gotoxy(frames.header.width - longestValue - 7, 1);
		frames.header.putmsg(
			format(
				"%sGroup: %s%s",
				settings.fhc,
				settings.fvc,
				msg_area.sub[settings.sub].grp_name
			)
		);
		frames.header.gotoxy(frames.header.width - longestValue - 6, 2);
		frames.header.putmsg(
			format(
				"%sArea: %s%s",
				settings.fhc, settings.fvc,	settings.sub
			)
		);
		frames.header.gotoxy(frames.header.width - longestValue - 5, 3);
		frames.header.putmsg(
			format(
				"%sMsg: %s%s%s/%s%s",
				settings.fhc, settings.fvc,	settings.msg,
				settings.fhc, settings.fvc,	last
			)
		);
		// This is a bit crazy, but the obvious alternatives don't work very well
		if(typeof frames.message != "undefined")
			frames.message.delete();
		frames.message = new Frame(
			frames.top.x,
			frames.top.y + frames.header.height,
			frames.top.width,
			frames.top.height - frames.header.height - frames.footer.height,
			LIGHTGRAY,
			frames.top
		);
		frames.message.putmsg(word_wrap(state.body, frames.message.width - 1, 79, true));
		frames.message.open();
		while(frames.message.offset.y > 0)
			frames.message.scroll(0, -1);
		scroller = new ScrollBar(frames.message, { autohide : true });
		scroller.open();
	}
	
	var init = function() {
		initFrames();
		showMessage();
	}

	var getNextMessage = function() {
		var next = settings.msg;
		var mb = new MsgBase(settings.sub);
		mb.open();
		for(var m = settings.msg + 1; m <= mb.last_msg; m++) {
			var i = mb.get_msg_index(m);
			if(i === null || i.attr&MSG_DELETE)
				continue;
			next = m;
			break;
		}
		mb.close();
		return next;
	}

	var getPreviousMessage = function() {
		var perv = settings.msg;
		var mb = new MsgBase(settings.sub);
		mb.open();
		for(var m = settings.msg - 1; m >= mb.first_msg; m--) {
			var i = mb.get_msg_index(m);
			if(i === null || i.attr&MSG_DELETE)
				continue;
			perv = m;
			break;
		}
		mb.close();
		return perv;
	}

	this.getcmd = function(userInput) {

		var num = settings.msg;
		var ret = true;
		switch(userInput.toUpperCase()) {
			case "Q":
				ret = false;
				break;
			case "R":
				if(!msg_area.sub[settings.sub].can_post)
					break;
				console.clear(7);
				var f = new File(system.node_dir + "QUOTES.TXT");
				f.open("w");
				f.write(state.body);
				f.close();
				bbs.post_msg(settings.sub, WM_QUOTE, state.header);
				if(typeof frames.top.parent != "undefined")
					frames.top.parent.invalidate();
				else
					frames.top.invalidate();
				break;
			case "P":
				if(!msg_area.sub[settings.sub].can_post)
					break;
				console.clear(7);
				bbs.post_msg(settings.sub);
				if(typeof frames.top.parent != "undefined")
					frames.top.parent.invalidate();
				else
					frames.top.invalidate();
				break;
			case KEY_UP:
				if(frames.message.data_height > frames.message.height)
					frames.message.scroll(0, -1);
				break;
			// KEYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYDOWWWWWWWWWWWWN six meters.
			case KEY_DOWN:
				if(frames.message.data_height > frames.message.height)
					frames.message.scroll(0, 1);
				break;
			case "[":
				if(frames.message.data_height > frames.message.height)
					frames.message.scroll(0, -frames.message.height);
				break;
			case "]":
				if(frames.message.data_height > frames.message.height);
					frames.message.scroll(0, Math.min(frames.message.data_height - frames.message.offset.y, frames.message.height));
				break;
			case KEY_LEFT:
				settings.msg = getPreviousMessage();
				break;
			case KEY_RIGHT:
				settings.msg = getNextMessage();
				break;
			// These only work as long as header thread_* are reliable and
			// point at existing messages. The alternative is probably too
			// slow to bother with.
			case "<":
				if(state.header.thread_back != 0 && state.thread_back_index !== null)
					settings.msg = state.header.thread_back;
				break;
			// Try for thread_next if it exists
			case ">":
				if(state.header.thread_next != 0 && state.thread_next_index !== null) {
					settings.msg = state.header.thread_next;
					break;
				}
			// Fall through to thread_first if not
			case ".":
				if(state.header.thread_first != 0 && state.thread_first_index !== null)
					settings.msg = state.header.thread_first;
				break;				
			default:
				break;
		}

		if(settings.msg != num)
			showMessage();
		return ret;

	}

	this.cycle = function() {
		if(typeof frames.top.parent != "undefined") {
			scroller.cycle();
		} else if(frames.top.cycle()) {
			scroller.cycle();
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
	}

	this.close = function() {
		frames.top.close();
		frames.top.delete();
	}

	init();

}
