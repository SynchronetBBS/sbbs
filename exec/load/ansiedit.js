/*
	Javascript ANSI Editor for Synchronet BBS
	by echicken -at- bbs.electronicchicken.com
	
	Object:
	
	ansiEdit(x, y, width, height, attributes, frame)

		- 'x' and 'y' are coordinates for the top left corner of the editor
		- 'width' and 'height' are the dimensions of the editor in characters
		  (Things will start breaking if you set these values too low)
		- 'attributes' are the default fg/bg colours (see sbbsdefs.js)
		- 'frame' is a parent Frame object (see frame.js) to put this ANSI
		  editor in, if any. (Optional)
	
	Methods:
	
	ansiEdit.getcmd(str)
	
		- Acts upon 'str' (eg. a return value from console.inkey()) to draw
		  a character, move the cursor, or bring up the pop-up menu.
							  
		- Returns an object with the following properties:
							  
		  x    : x coordinate of the character drawn
		  y    : y coordinate of the character drawn
		  ch   : The character that was drawn, false if nothing was drawn
		  attr : The colour attributes of the character that was drawn
	
	ansiEdit.putChar(charObject)
	
		- Draws charObject on the canvas, where charObject is an object
		  with properties x, y, ch, and attr, meaning where to draw it, what
		  to draw, and what colour to draw it in.  (See sbbsdefs.js for
		  more on colour attributes.)

	ansiEdit.cycle()
	
		- Update the ANSI editor's canvas (ansiEdit.getcmd(str) does this
		  automatically, but you will need to call this after calls to
		  ansiEdit.putChar().)
		  
	ansiEdit.open()
	
		- Only needed if you've used ansiEdit.close() already. New ANSI editor
		  objects open automagically.
		
	ansiEdit.close()
	
		- Close the ANSI editor and remove it from the screen.
		
	ansiEdit.showUI(boolean)
		
		- Show or hide the cursor and character set (must ansiEdit.cycle() or
		  cycle the parent frame for change to appear on screen.)
		  	
	Example:
	
	// Create an ANSI Editor at 1, 1, 80 columns wide and 23 lines long, with
	// the colours set to light grey on black to start.  Draw the letter 'a'
	// in white on blue at column 1 row 1 of the editor.  Loop until the user
	// hits <esc> (ascii(27)) and pass all user input to the ANSI Editor to
	// be processed (a.cycle()).  Dump the return value (c) of a.cycle() at
	// column 1, row 24 of the console.
	
	load("ansiedit.js");
	var a = new ansiEdit(1, 1, 80, 23, BG_BLACK|LIGHTGRAY);
	var b = "";
	var c;
	a.putChar( { x : 1, y : 1, ch : "a",  attr : BG_BLUE|WHITE } );
	while(ascii(b) != 27) {
		b = console.inkey(K_NONE, 5);
		if(b == "") continue;
		c = a.getcmd(b);
		a.cycle();
		if(!c.ch) continue; // No character was drawn
		console.gotoxy(1, 23);
		console.putmsg("\1h\1w" + c.toSource());
	}
*/

load("sbbsdefs.js");
load("frame.js");
load("tree.js");

function ansiEdit(x, y, width, height, attr, frame) {
	
	var str = "";
	var esc = ascii(27);
	var block = ascii(219);
	var canvasPos = { x : 1, y : 1 };

	var fgColour = 2;
	var bgColour = 0;
	var fgColours = [
		BLACK,
		DARKGRAY,
		LIGHTGRAY,
		WHITE,
		GREEN,
		LIGHTGREEN,
		BLUE,
		LIGHTBLUE,
		CYAN,
		LIGHTCYAN,
		MAGENTA,
		LIGHTMAGENTA,
		RED,
		LIGHTRED,
		BROWN,
		YELLOW
	];
	var bgColours = [
		BG_BLACK,
		BG_LIGHTGRAY,
		BG_BLUE,
		BG_CYAN,
		BG_GREEN,
		BG_MAGENTA,
		BG_RED,
		BG_BROWN
	];
	var currentAttributes = fgColours[fgColour]|bgColours[bgColour];

	var characterSet = 6;
	var characterSets = [
		["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
		[ascii(218), ascii(191), ascii(192), ascii(217), ascii(196), ascii(179), ascii(195), ascii(180), ascii(193), ascii(194)],
		[ascii(201), ascii(187), ascii(200), ascii(188), ascii(205), ascii(186), ascii(204), ascii(185), ascii(202), ascii(203)],
		[ascii(213), ascii(184), ascii(212), ascii(190), ascii(205), ascii(179), ascii(198), ascii(181), ascii(207), ascii(209)],
		[ascii(214), ascii(183), ascii(211), ascii(189), ascii(196), ascii(199), ascii(199), ascii(182), ascii(208), ascii(210)],
		[ascii(197), ascii(206), ascii(216), ascii(215), ascii(128), ascii(129), ascii(130), ascii(131), ascii(132), ascii(133)],
		[ascii(176), ascii(177), ascii(178), ascii(219), ascii(223), ascii(220), ascii(221), ascii(222), ascii(254), ascii(134)],
		[ascii(135), ascii(136), ascii(137), ascii(138), ascii(139), ascii(140), ascii(141), ascii(142), ascii(143), ascii(144)],
		[ascii(145), ascii(146), ascii(147), ascii(148), ascii(149), ascii(150), ascii(151), ascii(152), ascii(153), ascii(154)],
		[ascii(155), ascii(156), ascii(157), ascii(158), ascii(159), ascii(160), ascii(161), ascii(162), ascii(163), ascii(164)],
		[ascii(165), ascii(166), ascii(167), ascii(168), ascii(171), ascii(172), ascii(173), ascii(174), ascii(175), ascii(224)],
		[ascii(225), ascii(226), ascii(227), ascii(228), ascii(229), ascii(230), ascii(231), ascii(232), ascii(233), ascii(234)],
		[ascii(235), ascii(236), ascii(237), ascii(238), ascii(239), ascii(240), ascii(241), ascii(242), ascii(243), ascii(244)],
		[ascii(245), ascii(246), ascii(247), ascii(248), ascii(249), ascii(250), ascii(251), ascii(252), ascii(253)]
	]
	
	var	aFrame = new Frame(x, y, width, height, BG_BLACK, frame);
	var popUp = new Frame(parseInt((aFrame.width - 28) / 2), y, 28, aFrame.height - 1, BG_BLUE|WHITE, aFrame);
	var subPopUp = new Frame(popUp.x + 2, popUp.y + 1, popUp.width - 4, popUp.height - 2, BG_BLACK, popUp);
	var palette = new Frame(parseInt((aFrame.width - 36) / 2), parseInt((aFrame.height - 6) / 2), 36, 6, BG_BLUE|WHITE, aFrame);
	var subPalette = new Frame(palette.x + 2, palette.y + 1, palette.width - 4, palette.height - 2, BG_BLACK, palette);
	var pfgCursor = new Frame(x, y, 1, 1, BG_BLACK|WHITE, subPalette);
	var pbgCursor = new Frame(x, y, 1, 1, BG_BLACK|WHITE, subPalette);
	var canvas = new Frame(x, y, aFrame.width, aFrame.height - 1, BG_BLACK|LIGHTGRAY, aFrame);
	var cursor = new Frame(x, y, 1, 1, BG_BLACK|WHITE, canvas);
	var charSet = new Frame(x, (aFrame.y + aFrame.height - 1), aFrame.width, 1, currentAttributes, aFrame);

	charSet.update = function(index) {
		characterSet = index;
		var chrs = "";
		for(var c = 0; c < characterSets[characterSet].length; c++) {
			((c + 1) > 9) ? n = 0 : n = c + 1;
			chrs += n + ":" + characterSets[characterSet][c] + " ";
		}
		chrs += "<TAB> menu";
		charSet.attr = currentAttributes;
		charSet.clear();
		charSet.putmsg(chrs);
		return "EXITTREE";
	}
	
	colourPicker = function() {
		subPalette.clear();
		for(var c = 0; c < fgColours.length; c++) {
			var ch = (c == 0) ? "\1h\1w" + ascii(250) : ascii(219);
			subPalette.attr = fgColours[c];
			subPalette.putmsg(ch + ch);
			subPalette.attr = 0;
		}
		subPalette.gotoxy(1, 3);
		for(var c  = 0; c < bgColours.length; c++) {
			var ch = (c == 0) ? "\1h\1w" + ascii(250) : " ";
			subPalette.attr = bgColours[c];
			subPalette.putmsg(ch + ch + ch + ch);
			subPalette.attr = 0;
		}
		palette.top();
		pfgCursor.top();
		pbgCursor.top();
		var userInput = "";
		pfgCursor.moveTo((fgColour * 2) + subPalette.x, subPalette.y + 1);
		pbgCursor.moveTo((bgColour * 4) + subPalette.x, subPalette.y + 3);
		aFrame.cycle();
		while(!js.terminated) {
			var userInput = console.inkey(K_NONE, 5);
			if(userInput == "") continue;
			switch(userInput) {
				case KEY_LEFT:
					if(pfgCursor.x > subPalette.x) {
						pfgCursor.move(-2, 0);
						fgColour = fgColour - 1;
					} else {
						pfgCursor.moveTo(subPalette.x + subPalette.width - 2, pfgCursor.y);
						fgColour = fgColours.length - 1;
					}
					break;
				case KEY_RIGHT:
					if(pfgCursor.x < subPalette.x + subPalette.width - 2) {
						pfgCursor.move(2, 0);
						fgColour = fgColour + 1;
					} else {
						pfgCursor.moveTo(subPalette.x, pfgCursor.y);
						fgColour = 0;
					}
					break;
				case KEY_DOWN:
					if(pbgCursor.x > subPalette.x) {
						pbgCursor.move(-4, 0);
						bgColour = bgColour - 1;
					} else {
						pbgCursor.moveTo(subPalette.x + subPalette.width - 4, pbgCursor.y);
						bgColour = bgColours.length - 1;
					}
					break;
				case KEY_UP:
					if(pbgCursor.x < subPalette.x + subPalette.width - 4) {
						pbgCursor.move(4, 0);
						bgColour = bgColour + 1;
					} else {
						pbgCursor.moveTo(subPalette.x, pbgCursor.y);
						bgColour = 0;
					}
					break;
				default:
					break;
			}
			currentAttributes = fgColours[fgColour]|bgColours[bgColour];
			charSet.update(characterSet);
			if(aFrame.cycle()) console.gotoxy(80, 24);
			if(ascii(userInput) == 27 || ascii(userInput) == 13 || ascii(userInput) == 9) break;
		}
		palette.bottom();
		pfgCursor.bottom();
		pbgCursor.bottom();
		popUp.top();
		return "EXITTREE";
	}
	
	this.download = function() {
		charSet.close();
		popUp.close();
		aFrame.cycle();
		var f = system.data_dir + format("user/%04u.bin", user.number);
		canvas.screenShot(f, false);
		bbs.send_file(f, user.download_protocol);
		aFrame.close();
		console.clear();
		aFrame.open();
		aFrame.draw();
		return "EXITTREE";
	}
	
	var tree = new Tree(subPopUp);
	tree.colors.fg = WHITE;
	tree.colors.lfg = WHITE;
	tree.colors.lbg = BG_CYAN;
	tree.colors.tfg = LIGHTCYAN;
	tree.colors.xfg = LIGHTCYAN;
	tree.addItem("Color Palette", colourPicker);
	var charSetTree = tree.addTree("Choose Character Set");
	for(var c = 0; c < characterSets.length; c++) charSetTree.addItem(characterSets[c].join(" "), charSet.update, c);
	tree.addItem("Download this ANSI", this.download);
	tree.addItem("Clear the Canvas", "CLEAR");
	
	pfgCursor.putmsg(ascii(219));
	pbgCursor.putmsg(ascii(219));
	popUp.gotoxy(7, 1);
	popUp.putmsg("ANSI Editor Menu");
	popUp.gotoxy(1, popUp.height);
	popUp.putmsg("<ENTER> select | <TAB> close");
	palette.gotoxy(13, 1);
	palette.putmsg("Color Palette");
	palette.gotoxy(5, 6);
	palette.putmsg(ascii(17) + " Foreground " + ascii(16) + " " + ascii(30) + " Background " + ascii(31));
	cursor.putmsg(ascii(219));
	charSet.update(characterSet);

	aFrame.open();
	tree.open();
	aFrame.cycle();

	this.putChar = function(ch) {
		if(ch.ch == "CLEAR") {
			canvas.clear();
		} else {
			canvas.setData(ch.x - 1, ch.y - 1, ch.ch, ch.attr, false);
		}
	}
	
	this.cycle = function() {
		return aFrame.cycle();
	}
	
	this.showUI = function(s) {
		if(s) {
			cursor.top();
			charSet.top();
		} else {
			cursor.bottom();
			charSet.bottom();
		}
	}
	
	this.getcmd = function(str) {
		var retval = { x : canvasPos.x, y : canvasPos.y, ch : false, attr : currentAttributes }
		if(str == "") return retval;
		var cont = false;
		var num = Number(str);
		var asc = ascii(str);
		
		switch(str) {
			case KEY_UP:
				if(canvasPos.y > 1) canvas.gotoxy(canvasPos.x, canvasPos.y - 1);
				cont = true;
				break;
			case KEY_RIGHT:
				if(canvasPos.x < canvas.width) canvas.gotoxy(canvasPos.x + 1, canvasPos.y);
				cont = true;
				break;
			case KEY_DOWN:
				if(canvasPos.y < canvas.height) canvas.gotoxy(canvasPos.x, canvasPos.y + 1);
				cont = true;
				break
			case KEY_LEFT:
				if(canvasPos.x > 1) canvas.gotoxy(canvasPos.x - 1, canvasPos.y);
				cont = true;
				break;
			case KEY_HOME:
				canvas.gotoxy(1, canvasPos.y);
				break;
			case KEY_END:
				canvas.gotoxy(canvas.width, canvasPos.y);
				break;
			case "0":
				retval.ch = characterSets[characterSet][9];
				cont = true;
				break;
			default:
				break;
		}
		
		if(!cont && num != NaN && num > 0) {
			retval.ch = characterSets[characterSet][num - 1];
			cont = true;
		}
		
		if(!cont && asc > 31 && asc < 127) {
			retval.ch = str;
			cont = true;
		}
		
		if(!cont && asc == 13 && canvasPos.y < canvas.height) {
			canvas.gotoxy(1, canvasPos.y + 1);
			cont = true;
		}
		
		if(!cont && asc == 8 && canvasPos.x > 1) {
			retval.x = canvasPos.x - 1;
			retval.ch = " ";
			canvas.gotoxy(canvasPos.x - 1, canvasPos.y);
			canvas.attr = BG_BLACK|BLACK;
			canvas.putmsg(" ");
			canvas.gotoxy(canvasPos.x - 1, canvasPos.y);
			cont = false;
		}
		
		if(!cont && asc == 9) {
			popUp.top();
			aFrame.cycle();
			var userInput = "";
			while(ascii(userInput) != 27 && ascii(userInput) != 9) {
				userInput = console.inkey(K_NONE, 5);
				if(userInput == "") continue;
				var ret = tree.getcmd(userInput);
				if(ret == "EXITTREE") break;
				if(ret == "CLEAR") {
					canvas.clear();
					retval.ch = "CLEAR";
					break;
				}
				if(popUp.cycle()) console.gotoxy(80, 24);
			}
			popUp.bottom();
		}

		if(cont && retval.ch !== false) {
			canvas.attr = currentAttributes;
			canvas.putmsg(retval.ch);
			canvas.attr = BG_BLACK|BLACK;
		}

		if(canvas.getxy().x > canvas.width || canvas.getxy().y > canvas.height) canvas.gotoxy(canvasPos.x, canvasPos.y);
		canvasPos = canvas.getxy();
		cursor.moveTo(canvas.x + canvas.cursor.x, canvas.y + canvas.cursor.y);
		if(aFrame.cycle()) console.gotoxy(80, 24);
		return retval;
	}

	this.open = function() {
		aFrame.open();
		return;
	}
	
	this.close = function() {
		aFrame.close();
		return;
	}
	
}
