load("frame.js");
load("inputline.js");

var frame,
	onelinersFrame,
	inputFrame,
	inputLine,
	oneliners,
	inInput = false,
	attr = console.attributes,
	status = bbs.sys_status;

var initFrames = function() {

	console.clear(BG_BLACK|LIGHTGRAY);

	Frame.prototype.drawBorder = function(color) {
		var theColor = color;
		if(Array.isArray(color));
			var sectionLength = Math.round(this.width / color.length);
		this.pushxy();
		for(var y = 1; y <= this.height; y++) {
			for(var x = 1; x <= this.width; x++) {
				if(x > 1 && x < this.width && y > 1 && y < this.height)
					continue;
				var msg;
				this.gotoxy(x, y);
				if(y == 1 && x == 1)
					msg = ascii(218);
				else if(y == 1 && x == this.width)
					msg = ascii(191);
				else if(y == this.height && x == 1)
					msg = ascii(192);
				else if(y == this.height && x == this.width)
					msg = ascii(217);
				else if(x == 1 || x == this.width)
					msg = ascii(179);
				else
					msg = ascii(196);
				if(Array.isArray(color)) {
					if(x == 1)
						theColor = color[0];
					else if(x % sectionLength == 0 && x < this.width)
						theColor = color[x / sectionLength];
					else if(x == this.width)
						theColor = color[color.length - 1];
				}
				this.putmsg(msg, theColor);
			}
		}
		this.popxy();
	}

	frame = new Frame(
		1,
		1,
		console.screen_columns,
		console.screen_rows,
		LIGHTGRAY
	);

	onelinersFrame = new Frame(
		frame.x + 1,
		frame.y + 2,
		frame.width - 2,
		frame.height - 4,
		LIGHTGRAY,
		frame
	);
	onelinersFrame.v_scroll = true;

	inputAliasFrame = new Frame(
		onelinersFrame.x,
		frame.height - 1,
		user.alias.length + system.qwk_id.length + 3,
		1,
		LIGHTGRAY,
		frame
	);

	inputFrame = new Frame(
		onelinersFrame.x + user.alias.length + system.qwk_id.length + 3,
		frame.height - 1,
		onelinersFrame.width - user.alias.length - system.qwk_id.length - 3,
		1,
		BG_BLUE|LIGHTGRAY,
		frame
	);

	promptFrame = new Frame(
		onelinersFrame.x,
		frame.height - 1,
		onelinersFrame.width,
		1,
		LIGHTCYAN,
		frame
	);

	inputAliasFrame.putmsg(user.alias + "\1n\1c@\1b" + system.qwk_id.toLowerCase() + "\1h\1k: ");
	promptFrame.putmsg("Add a oneliner to the wall? [Y/\1h\1wN\1h\1c]");
	frame.drawBorder([LIGHTBLUE, CYAN, LIGHTCYAN, WHITE]);
	var header = format(
		"\1h\1w%s synchronet oneliners %s",
		ascii(180), ascii(195)
	);
	frame.gotoxy(frame.width - console.strlen(header) - 2, 1);
	frame.putmsg(header);
	frame.open();

}

var initInput = function() {
	inputLine = new InputLine(inputFrame);
	inputLine.max_buffer = inputFrame.width;
	inputLine.auto_clear = false;
	inputLine.attr = BG_BLUE|WHITE;
	inputLine.cursor_attr = BG_BLUE|WHITE;
}

var initJSON = function() {
	try {
		oneliners = new Oneliners(settings.server, settings.port, putOneliner);
		var count = oneliners.count;
		if(count > onelinersFrame.height)
			var lines = oneliners.read(count - onelinersFrame.height);
		else
			var lines = oneliners.read(0);
		for(var line in lines)
			putOneliner(lines[line]);
		oneliners.callback = putOneliner;
	} catch(err) {
		log(LOG_ERR, "Oneliners error: " + err);
		exit();
	}
}

var init = function() {
	bbs.sys_status |= SS_MOFF;
	initFrames();
	initInput();
	initJSON();
}

var putOneliner = function(oneliner) {
	oneliner.oneliner = strip_exascii(oneliner.oneliner);
	if(oneliner.oneliner.length < 1)
		return;
	onelinersFrame.putmsg(
		format(
			"\1n\1w%s\1n\1c@\1h\1b%s\1h\1k: \1n\1w%s\r\n",
			oneliner.alias,
			oneliner.qwkid,
			pipeToCtrlA(
				(	oneliner.oneliner.length
					+
					oneliner.alias.length
					+
					oneliner.qwkid.length
					+
					3
					>
					onelinersFrame.width
				)
				?
				oneliner.oneliner.substr(
					0,
					onelinersFrame.width
					-
					(	oneliner.alias.length
						+
						oneliner.qwkid.length
						+
						3
					)
				)
				:
				oneliner.oneliner
			)
		)
	);
	if(onelinersFrame.data_height > onelinersFrame.height)
		onelinersFrame.scroll(0, 1);
}

var cycle = function() {
	try {
		oneliners.cycle();
	} catch(err) {
		log(LOG_ERR, "Oneliners error: " + err);
	}
	if(frame.cycle() && !inInput)
		console.gotoxy(console.screen_columns, console.screen_rows);
	return (inInput)?inputLine.getkey():console.inkey(K_NONE,5).toUpperCase();
}

var main = function() {
	var userInput;
	while(!js.terminated) {
		userInput = cycle();
		if(inInput) {
			inputLine.attr =
				(inputLine.buffer.length > (80 - user.alias.length - system.qwk_id.length - 3))
				?
				BG_BLUE|RED
				:
				BG_BLUE|WHITE;
			if(typeof userInput != "undefined") {
				inInput = false;
				inputLine.clear();
				promptFrame.open();
				promptFrame.top();
				userInput = userInput.replace(/\\1/g, ascii(1));
				if(console.strlen(userInput) < 1)
					continue;
				postOneliner(user.alias, userInput);
			}
			continue;
		}
		if(!inInput) {
			userInput = userInput.toUpperCase();
			if(userInput == "Y") {
				promptFrame.close();
				inputAliasFrame.top();
				inputFrame.top();
				inInput = true;
				// A hack to get the cursor (near) to the correct spot
				console.ungetstr("*");
				inputLine.getkey();
				console.ungetstr("\x08");
				// So ends the hack.
				continue;
			} else if(userInput == "N" || ascii(userInput) == 13) {
				break;
			}
			continue;
		}
	}
}

var cleanUp = function() {
	try {
		oneliners.close();
	} catch(err) {
		log(LOG_ERR, "Oneliners error: " + err);
	}
	frame.close();
	console.clear(attr);
	bbs.sys_status = status;
}

init();
main();
cleanUp();
