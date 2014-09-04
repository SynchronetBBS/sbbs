/*	"ANSI graphics" editor object
	by echicken -at- bbs.electronicchicken.com

	Embed an ANSI graphics editor in an application.
	See xtrn/syncwall/ for a demo, or try the following sample code:

	Working example:

		load("ansiedit.js");
		console.clear(LIGHTGRAY);
		var ansiEdit = new ANSIEdit(
			{	'x' : 1,
				'y' : 1,
				'width' : console.screen_columns,
				'height' : console.screen_rows
			}
		);
		ansiEdit.open();
		while(!js.terminated) {
			var userInput = console.inkey(K_NONE, 5);
			if(ascii(userInput) == 27)
				break;
			ansiEdit.getcmd(userInput);
		}
		ansiEdit.close();
		console.clear(LIGHTGRAY);

	Instantiation:

		Where 'options' is an object with the following properties:

		- 'x', X coordinate of top left corner of editor (number) (required)
		- 'y', Y coordinate of top left corner of editor (number) (required)
		- 'width', Width of the editor (number) (required)
		- 'height', Height of the editor (number) (required)
		- 'parentFrame', Frame object this a child of (Frame) (optional)
		- 'vScroll', Vertical scrolling (boolean) (optional)
		- 'hScroll', Horizontal scrolling (boolean) (optional)
		- 'showPosition', Show cursor position readout (boolean) (optional)
		- 'menuHeading', Text at the top of the <TAB> menu (string) (optional)

		var ansiEdit = new ANSIEdit(options);

	Properties:

		.menu
			An instance of Tree (see exec/load/tree.js for usage)
			Use this to add items to the <TAB> menu

	Methods:

		.open() (void)

			Initialize the editor and display it.
			If nesting the editor inside of a parent Frame object, this must
			be called after [parentFrame].open().

		.close() (void)

			Close the editor and remove it from display.

		.clear() (void)

			Clear the contents of the editor.

		.getcmd(string) (object)

			Handle already-received user input, where 'string' should be a
			single character/keypress, eg. the return value of console.inkey().

			Returns an object with the following properties:

			- 'x', The x coordinate of the cursor (number)
			- 'y', The y coordinate of the cursor (number)
			- 'ch'	- The character that was drawn (string) or false if none
					- or "CLEAR" if the editor was cleared
			- 'attr' - The console attributes (color, intensity) at this position

		.putChar({'x' : number, 'y' : number, 'ch' : string, 'attr' : number}) (void)

			Put character 'ch' at 'x' and 'y' with attributes 'attr'.

		.cycle() (void)

			This is called automatically from ANSIEdit.getcmd(). It refreshes
			the editor's frames, places the console cursor where it should be,
			and updates the cursor position display.  If your editor is nested
			inside another frame or you are moving the cursor outside of the
			editor, you may need to call this directly.

		To do:

			- Block cut/copy/paste operations
			- "iCe colors"? Meh.
			- Blink attr? (I don't think it works with frame.js presently.)

		Support:

			- Post a message to 'echicken' in Synchronet Javascript on DOVE-Net
			- Find me in #synchronet on irc.synchro.net
			- Send an email to echicken -at- bbs.electronicchicken.com
*/

load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("funclib.js");

var ANSIEdit = function(options) {

	var tree,
		frames = {},
		self = this,
		characterSets = [
			[ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" ],
			[ 218, 191, 192, 217, 196, 179, 195, 180, 193, 194 ],
			[ 201, 187, 200, 188, 205, 186, 204, 185, 202, 203 ],
			[ 213, 184, 212, 190, 205, 179, 198, 181, 207, 209 ],
			[ 214, 183, 211, 189, 196, 199, 199, 182, 208, 210 ],
			[ 197, 206, 216, 215, 128, 129, 130, 131, 132, 133 ],
			[ 176, 177, 178, 219, 223, 220, 221, 222, 254, 134 ],
			[ 135, 136, 137, 138, 139, 140, 141, 142, 143, 144 ],
			[ 145, 146, 147, 148, 149, 150, 151, 152, 153, 154 ],
			[ 155, 156, 157, 158, 159, 160, 161, 162, 163, 164 ],
			[ 165, 166, 167, 168, 171, 172, 173, 174, 175, 224 ],
			[ 225, 226, 227, 228, 229, 230, 231, 232, 233, 234 ],
			[ 235, 236, 237, 238, 239, 240, 241, 242, 243, 244 ],
			[ 245, 246, 247, 248, 249, 250, 251, 252, 253, 253 ]
		];

	var	state = {
		'attr' : (typeof options.attr != "number") ? LIGHTGRAY : options.attr,
		'inMenu' : false,
		'cursor' : { 'x' : 1, 'y' : 1 },
		'lastCursor' : { 'x' : 0, 'y' : 0 },
		'charSet' : 6,
	};

	var settings = {
		'vScroll' : (typeof options.vScroll != "boolean") ? false : options.vScroll,
		'hScroll' : (typeof options.hScroll != "boolean") ? false : options.hScroll,
		'showPosition' : (typeof options.showPosition != "boolean") ? false : true,
		'menuHeading' : (typeof options.menuHeading != "string") ? "ANSI Editor" : options.menuHeading
	};

	var attrMap = {};
	attrMap[HIGH] = 1;
	attrMap[BLINK] = 5;
	attrMap[BLACK] = 30;
	attrMap[DARKGRAY] = 30;
	attrMap[RED] = 31;
	attrMap[LIGHTRED] = 31;
	attrMap[GREEN] = 32;
	attrMap[LIGHTGREEN] = 32;
	attrMap[BROWN] = 33;
	attrMap[YELLOW] = 33;
	attrMap[BLUE] = 34;
	attrMap[LIGHTBLUE] = 34;
	attrMap[MAGENTA] = 35;
	attrMap[LIGHTMAGENTA] = 35;
	attrMap[CYAN] = 36;
	attrMap[LIGHTCYAN] = 36;
	attrMap[LIGHTGRAY] = 37;
	attrMap[WHITE] = 37;
	attrMap[BG_BLACK] = 40;
	attrMap[BG_RED] = 41;
	attrMap[BG_GREEN] = 42;
	attrMap[BG_BROWN] = 43;
	attrMap[BG_BLUE] = 44;
	attrMap[BG_MAGENTA] = 45;
	attrMap[BG_CYAN] = 46;
	attrMap[BG_LIGHTGRAY] = 47;

	var initFrames = function() {

		frames.top = new Frame(
			options.x,
			options.y,
			options.width,
			options.height,
			LIGHTGRAY,
			options.parentFrame
		);

		frames.canvas = new Frame(
			frames.top.x,
			frames.top.y,
			frames.top.width,
			frames.top.height - 1,
			LIGHTGRAY,
			frames.top
		);
		frames.canvas.v_scroll = settings.vScroll;
		frames.canvas.h_scroll = settings.hScroll;

		frames.charSet = new Frame(
			frames.top.x,
			frames.canvas.y + frames.canvas.height,
			50,
			1,
			state.attr,
			frames.top
		);

		if(settings.showPosition) {
			frames.position = new Frame(
				frames.charSet.x + frames.charSet.width + 1,
				frames.charSet.y,
				frames.top.width - frames.charSet.width - 1,
				1,
				state.attr,
				frames.top
			);
		}

		frames.menu = new Frame(
			Math.floor((frames.top.width - 28) / 2),
			frames.canvas.y,
			28,
			frames.canvas.height,
			BG_BLUE|WHITE,
			frames.top
		);
		frames.menu.center(settings.menuHeading);
		frames.menu.gotoxy(1, frames.menu.height);
		frames.menu.center("Press <TAB> to exit");

		frames.subMenu = new Frame(
			frames.menu.x + 1,
			frames.menu.y + 1,
			frames.menu.width - 2,
			frames.menu.height - 2,
			WHITE,
			frames.menu
		);

	}

	var initMenu = function() {
		tree = new Tree(frames.subMenu);
		tree.colors.fg = WHITE;
		tree.colors.lfg = WHITE;
		tree.colors.lbg = BG_CYAN;
		tree.colors.tfg = LIGHTCYAN;
		tree.colors.xfg = LIGHTCYAN;
		tree.addItem("Color Palette", setColour);
		var charSetTree = tree.addTree("Choose Character Set");
		charSetTree.addItem(characterSets[0].join(" "), drawCharSet, 0);
		for(var c = 1; c < characterSets.length; c++) {
			var line = [];
			for(var cc in characterSets[c])
				line.push(ascii(characterSets[c][cc]));
			charSetTree.addItem(line.join(" "), drawCharSet, c);
		}
		var downloadTree = tree.addTree("Download this ANSI");
		downloadTree.addItem(" Binary", download, "bin");
		downloadTree.addItem(" ASCII", download, "ascii");
		downloadTree.addItem(" ANSI", download, "ansi");
		tree.addItem("Clear the Canvas", self.clear);
		tree.open();
	}

	var setColour = function() {
		state.attr = colorPicker(
			frames.menu.x - 4,
			Math.floor((frames.menu.height - 6) / 2),
			frames.menu,
			state.attr
		);
		drawCharSet();
		return "DONE";
	}

	var drawCharSet = function(n) {
		if(typeof n != "undefined")
			state.charSet = n;
		frames.charSet.attr = state.attr;
		frames.charSet.clear();
		for(var c = 0; c < characterSets[state.charSet].length; c++)
			frames.charSet.putmsg(((c+1)%10) + ":" + ascii(characterSets[state.charSet][c]) + " ");
		frames.charSet.putmsg("<TAB> menu");
		return "DONE";
	}

	var saveAscii = function() {
		var lines = [];
		log(frames.canvas.data_width);
		for(var y = 0; y < frames.canvas.data_height; y++) {
			var line = "";
			for(var x = 0; x < frames.canvas.data_width; x++) {
				var ch = frames.canvas.getData(x, y);
				line += (typeof ch.ch == "undefined" || ch.ch == "") ? " " : ch.ch;
			}
			lines.push(line);
		}
		var fn = system.data_dir + format("user/%04u.asc", user.number);
		var f = new File(fn);
		f.open("w");
		f.writeAll(lines);
		f.close();
		return fn;
	}

	var saveAnsi = function() {
		var fgmask = (1<<0)|(1<<1)|(1<<2)|(1<<3);
		var bgmask = (1<<4)|(1<<5)|(1<<6);
		var lines = [], fg = 7, bg = 0, hi = 0;
		var line = format("%s[%s;%s;%sm", ascii(27), hi, fg, bg);
		for(var y = 0; y < frames.canvas.data_height; y++) {
			if(y > 0)
				line = "";
			var blanks = 0;
			for(var x = 0; x < frames.canvas.data_width; x++) {
				var ch = frames.canvas.getData(x, y);
				if(typeof ch.attr != "undefined") {
					var sequence = [];
					if((ch.attr&fgmask) != fg) {
						fg = (ch.attr&fgmask);
						if(fg > 7 && hi == 0) {
							hi = 1;
							sequence.push(hi);
						} else if(fg < 8 && hi == 1) {
							hi = 0;
							sequence.push(hi);
						}
						sequence.push(attrMap[fg]);
					}
					if((ch.attr&bgmask) != bg) {
						bg = (ch.attr&bgmask);
						sequence.push((bg == 0) ? 40 : attrMap[bg]);
					}
					sequence = format("%s[%sm", ascii(27), sequence.join(";"));
					if(sequence.length > 3)
						line += sequence;
				}
				if(typeof ch.ch == "undefined") {
					blanks++;
				} else {
					if(blanks > 0) {
						line += ascii(27) + "[" + blanks + "C";
						blanks = 0;
					}
					line += ch.ch;
				}
			}
			lines.push(line);
		}
		var fn = system.data_dir + format("user/%04u.ans", user.number);
		var f = new File(fn);
		f.open("w");
		f.writeAll(lines);
		f.close();
		return fn;
	}

	var saveBin = function() {
		var f = system.data_dir + format("user/%04u.bin", user.number);
		frames.canvas.screenShot(f, false);
		return f;
	}

	var download = function(type) {
		switch(type) {
			case "bin":
				var f = saveBin();
				break;
			case "ansi":
				var f = saveAnsi();
				break;
			case "ascii":
				var f = saveAscii();
				break;
			default:
				break;
		}
		if(typeof f != "undefined") {
			console.clear(LIGHTGRAY);
			bbs.send_file(f, user.download_protocol);
			frames.top.invalidate();
		}
		return "DONE";
	}

	var raiseMenu = function() {
		state.inMenu = true;
		frames.menu.top();
	}

	var lowerMenu = function() {
		state.inMenu = false;
		frames.menu.bottom();
	}

	this.open = function() {
		initFrames();
		initMenu();
		this.menu = tree; // Allow people to add menu items to an editor
		frames.top.open();
		frames.menu.bottom();
		drawCharSet();
		state.cursor.x = frames.canvas.x;
		state.cursor.y = frames.canvas.y;
	}

	this.close = function() {
		tree.close();
		frames.top.close();
		if(typeof frames.top.parent != "undefined")
			frames.top.parent.invalidate();
		frames.top.delete();
	}

	this.clear = function() {
		frames.canvas.clear();
		state.cursor.x = frames.canvas.x;
		state.cursor.y = frames.canvas.y;
		return "CLEAR";
	}

	this.putChar = function(ch) {

		// Hacky workaround for something broken in Frame.scroll()
		if(frames.canvas.data_height > frames.canvas.height && frames.canvas.offset.y > 0)
			frames.canvas.refresh();
		if(frames.canvas.data_width > frames.canvas.width && frames.canvas.offset.x > 0)
			frames.canvas.refresh();

		frames.canvas.setData(
			(typeof ch.x == "undefined") ? (state.cursor.x - frames.canvas.x) : ch.x,
			(typeof ch.y == "undefined") ? (state.cursor.y - frames.canvas.y) : ch.y,
			ch.ch,
			(typeof ch.attr == "undefined") ? state.attr : ch.attr
		);

		if(typeof ch.x != "undefined") {
			this.cycle(true);
			return;
		}

		var ret = {
			'x' : state.cursor.x - frames.canvas.x,
			'y' : state.cursor.y - frames.canvas.y,
			'ch' : ch.ch,
			'attr' : state.attr
		};

		if(	settings.hScroll
			||
			state.cursor.x < frames.canvas.x + frames.canvas.width - 1
		) {
			state.cursor.x++;
		}

		if(	settings.hScroll
			&&
			state.cursor.x - frames.canvas.offset.x > frames.canvas.width
		) {
			frames.canvas.scroll(1, 0);
		}

		return ret;

	}

	this.getcmd = function(userInput) {

		if(userInput == "\x09") {

			(state.inMenu) ? lowerMenu() : raiseMenu();

		} else if(state.inMenu) {

			var ret = tree.getcmd(userInput);
			if(ret == "DONE" || ret == "CLEAR")
				lowerMenu();
			if(ret == "CLEAR") {
				var retval = {
					'x' : state.cursor.x - frames.canvas.x,
					'y' : state.cursor.y - frames.canvas.y,
					'ch' : "CLEAR",
					'attr' : state.attr
				}
			}

		} else if(
			userInput.match(/[\r]/) !== null
			&&
			(state.cursor.y < frames.canvas.y + frames.canvas.height - 1 || settings.vScroll)
		) {

			state.cursor.y++;
			state.cursor.x = frames.canvas.x;
			if(state.cursor.y - frames.canvas.offset.y > frames.canvas.height) {
				// I don't know why I need to do this, especially not after looking at Frame.scroll()
				// but vertical Frame.scroll() won't work unless I do this
				if(typeof frames.canvas.data[state.cursor.y - frames.canvas.y] == "undefined") {
					frames.canvas.setData(
						state.cursor.x - frames.canvas.x,
						state.cursor.y - frames.canvas.y,
						"",
						0
					);
				}
				frames.canvas.scroll();
			}

		} else if(userInput == "\x08" && state.cursor.x > frames.canvas.x) {

			state.cursor.x--;
			var retval = this.putChar( { 'ch' : " "	});
			state.cursor.x--;

		} else if(userInput == "\x7F") {

			var retval = this.putChar({ 'ch' : " " });
			state.cursor.x--;

		} else if(userInput.match(/[0-9]/) !== null) {

			userInput = (userInput == 0) ? 9 : userInput - 1;
			var retval = this.putChar(
				{ 'ch' : ascii(characterSets[state.charSet][userInput]) }
			);

		} else if(userInput.match(/[\x20-\x7E]/) !== null) {

			var retval = this.putChar({	'ch' : userInput });

		} else {

			switch(userInput) {

				case KEY_UP:
					if(state.cursor.y > frames.canvas.y)
						state.cursor.y--;
					if(	settings.vScroll
						&&
						frames.canvas.offset.y > 0
						&&
						state.cursor.y == frames.canvas.offset.y
					) {
						frames.canvas.scroll(0, -1);
					}
					break;

				case KEY_DOWN:
					if(settings.vScroll || state.cursor.y < frames.canvas.y + frames.canvas.height - 1)
						state.cursor.y++;
					if(	settings.vScroll
						&&
						state.cursor.y - frames.canvas.offset.y > frames.canvas.height
					) {
						// I don't know why I need to do this, especially not after looking at Frame.scroll()
						// but vertical Frame.scroll() won't work unless I do this
						if(typeof frames.canvas.data[state.cursor.y - frames.canvas.y] == "undefined") {
							frames.canvas.setData(
								state.cursor.x - frames.canvas.x,
								state.cursor.y - frames.canvas.y,
								"",
								0
							);
						}
						frames.canvas.scroll();
					}
					break;

				case KEY_LEFT:
					if(state.cursor.x > frames.canvas.x)
						state.cursor.x--;
					if( settings.hScroll
						&&
						frames.canvas.offset.x > 0
						&&
						state.cursor.x == frames.canvas.offset.x
					) {
						frames.canvas.scroll(-1, 0);
					}
					break;

				case KEY_RIGHT:
					if(settings.hScroll || state.cursor.x < frames.canvas.x + frames.canvas.width - 1)
						state.cursor.x++;
					if( settings.hScroll
						&&
						state.cursor.x - frames.canvas.offset.x > frames.canvas.width
					) {
						// Hacky hack to populate entire frame data matrix
						// Horizontal Frame.scroll() doesn't otherwise work in this situation
						for(var y = 0; y <= state.cursor.y - frames.canvas.y; y++) {
							if(typeof frames.canvas.data[state.cursor.y - frames.canvas.y] == "undefined")
								frames.canvas.setData(state.cursor.x - frames.canvas.x, y, "", 0);
							for(var x = 0; x <= state.cursor.x - frames.canvas.x; x++) {
								if(typeof frames.canvas.data[y][x] == "undefined")
									frames.canvas.setData(x, y, "", 0);
							}
						}
						frames.canvas.scroll(1, 0);
					}
					break;

				case KEY_HOME:
					state.cursor.x = frames.canvas.x;
					break;

				case KEY_END:
					state.cursor.x = frames.canvas.data_width;
					break;

				default:
					break;

			}

		}

		this.cycle();

		return (typeof retval != "undefined") ?	retval : ({ 'x' : state.cursor.x - frames.canvas.x, 'y' : state.cursor.y - frames.canvas.y,	'ch' : false, 'attr' : state.attr });

	}

	this.cycle = function(force) {

		if(typeof parentFrame == "undefined")
			frames.top.cycle();

		if(	(typeof force == "boolean" && force)
			||
			(	state.cursor.x != state.lastCursor.x
				||
				state.cursor.y != state.lastCursor.y
			)
		) {

			console.attributes = state.attr;
			console.gotoxy(
				state.cursor.x - frames.canvas.offset.x,
				state.cursor.y - frames.canvas.offset.y
			);

			for(var c in state.cursor)
				state.lastCursor[c] = state.cursor[c];

			if(!settings.showPosition)
				return;

			frames.position.clear();
			frames.position.attr = state.attr;
			frames.position.putmsg(format("(%s:%s)", state.cursor.x, state.cursor.y));

		}

	}

}
