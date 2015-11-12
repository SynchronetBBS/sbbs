/*
 * Screen object...
 * This object is the estimate of what a screen looks like modeled
 * as a Graphic object (Screen.graphic).
 * 
 * The Graphic object should not be written directly, but instead,
 * all data should be provided using the print() method.
 */

if (js.global.Graphic === undefined)
	load("graphic.js");
if (js.global.Attribute === undefined)
	load("attribute.js");

function Screen(w, h, attr, fill)
{
	this.graphic = new Graphic(w, h, attr, fill);
	this.escbuf = '';
	this.pos = {x:0, y:0};
	this.stored_pos = {x:0, y:0};
	this.attr = new Attribute(7);
}
Screen.ANSIRe = /^(.*?)\x1b\[([<-\?]{0,1})([0-;]*)([ -\/]*)([@-~])([\x00-\xff]*)$/;
Screen.ANSIFragRe = /^(.*?)(\x1b(\[([<-\?]{0,1})([0-;]*)([ -\/]*)([@-~])?)?)$/;
Screen.StripColonRe = /:[^;:]*/g;

Screen.prototype.print=function(str) {
	var m;
	var ext;
	var pb;
	var ib;
	var fb;
	var p;
	var chars;
	var remain;
	var i;
	var tg;
	var seq;
	var x;
	var y;

	function writech(scr, ch) {
		var i;

		function check_scrollup(scr) {
			var i;

			while (scr.pos.y >= scr.graphic.height) {
				// Scroll up...
				scr.graphic.copy(0,1,scr.graphic.width-1, scr.graphic.height-2, 0, 0);
				for (i=0; i<scr.graphic.width; i++) {
					scr.graphic.data[i][scr.graphic.height-1].ch = scr.graphic.ch;
					scr.graphic.data[i][scr.graphic.height-1].attr = scr.attr.value;
				}
				scr.pos.y--;
			}
		}

		// Handle special chars.
		switch(ch) {
			case '\x00':	// NUL is not displayed.
				break;
			case '\x07':	// Beep is not displayed.
				break;
			case '\x08':	// Backspace.
				scr.pos.x--;
				if (scr.pos.x < 0)
					scr.pos.x = 0;
				break;
			case '\x09':	// Tab
				do {
					scr.pos.x++;
				} while(scr.pos.x % 8);
				if (scr.pos.x >= scr.graphic.width)
					scr.pos.x = scr.graphic.width-1;
				break;
			case '\x0a':	// Linefeed
				scr.pos.y++;
				check_scrollup(scr);
				break;
			case '\x0c':	// Form feed (clear screen and home)
				scr.graphic.clear();
				scr.pos.x=0;
				scr.pos.y=0;
				break;
			case '\x0d':	// Carriage return
				scr.pos.x = 0;
				break;
			default:
				scr.graphic.data[scr.pos.x][scr.pos.y].ch = ch;
				scr.graphic.data[scr.pos.x][scr.pos.y].attr.value = scr.attr.value;
				scr.pos.x++;
				if (scr.pos.x >= scr.graphic.width) {
					scr.pos.x = 0;
					scr.pos.y++;
					check_scrollup(scr);
				}
				break;
		}
	}

	function param_defaults(params, defaults) {
		var i;

		for (i=0; i<defaults.length; i++) {
			if (params[i] == undefined || params[i].length == 0)
				params[i]=defaults[i];
		}
		for (i=0; i<params.length; i++) {
			if (params[i]===undefined || params[i]==='')
				params[i] = 0;
			else
				params[i] = parseInt(params[i], 10);
		}
	}

	// Prepend the ESC buffer to avoid failing on split ANSI
	str = this.escbuf + str;
	this.escbuf = '';

	while((m=str.match(Screen.ANSIRe)) !== null) {
		chars = m[1];
		ext = m[2];
		pb = m[3];
		ib = m[4];
		fb = m[5];
		remain = m[6];
		seq = ext + ib + fb;

		str = remain;

		// Send regular chars before the sequence...
		for (i=0; i<chars.length; i++)
			writech(this, chars[i]);

		// We don't support any : paramters... strip and ignore.
		p = pb.replace(Screen.StripColonRe, '');
		p = p.split(';');

		switch(fb) {
			case '@':	// Insert character
				param_defaults(p, [1]);
				if (p[1] > this.graphic.width - this.pos.x)
					p[1] = this.graphic.width - this.pos.x;
				if (this.pos.x < this.graphic.width-1)
					this.graphic.copy(this.pos.x, this.pos.y, this.graphic.width-1 - p[1], this.pos.y, this.pos.x + p[1], this.pos.y);
				for (x = 0; x<p[1]; x++) {
					this.graphic.data[this.pos.x + x][this.pos.y].ch = this.graphic.ch;
					this.graphic.data[this.pos.x + x][this.pos.y].attr = this.attr.value;
				}
				break;
			case 'A':	// Cursor Up
				param_defaults(p, [1]);
				this.pos.y -= p[0];
				if (this.pos.y < 0)
					this.pos.y = 0;
				break;
			case 'B':	// Cursor Down
				param_defaults(p, [1]);
				this.pos.y += p[0];
				if (this.pos.y >= this.graphic.height)
					this.pos.y = this.graphic.height-1;
				break;
			case 'C':	// Cursor Right
				param_defaults(p, [1]);
				this.pos.x += p[0];
				if (this.pos.x >= this.graphic.width)
					this.pos.x = this.graphic.width-1;
				break;
			case 'D':	// Cursor Left
				param_defaults(p, [1]);
				this.pos.x -= p[0];
				if (this.pos.x < 0)
					this.pos.x = 0;
				break;
			case 'H':	// Cursor position
			case 'f':
				param_defaults(p, [1,1]);
				if (p[0] >= 0 && p[0] < this.graphic.height && p[1] >= 0 && p[1] <= this.graphic.width) {
					this.pos.x = p[1];
					this.pos.y = p[0];
				}
				break;
			case 'J':	// Erase in screen
				param_defaults(p, [0]);
				switch(p[0]) {
					case 0:	// Erase to end of screen...
						for (x = this.pos.x; x<this.pos.width; x++) {
							this.graphic.data[x][this.pos.y].ch = this.graphic.ch;
							this.graphic.data[x][this.pos.y].attr = this.attr.value;
						}
						for (y = this.pos.y+1; y<this.pos.height; y++) {
							for (x = 0; x<this.graphic.width; x++) {
								this.graphic.data[x][y].ch = this.graphic.ch;
								this.graphic.data[x][y].attr = this.attr.value;
							}
						}
						break;
					case 1:	// Erase to beginning of screen...
						for (y = 0; y < this.pos.y; y++) {
							for (x = 0; x<this.graphic.width; x++) {
								this.graphic.data[x][y].ch = this.graphic.ch;
								this.graphic.data[x][y].attr = this.attr.value;
							}
						}
						for (x = 0; x<=this.pos.x; x++) {
							this.graphic.data[x][this.pos.y].ch = this.graphic.ch;
							this.graphic.data[x][this.pos.y].attr = this.attr.value;
						}
						break;
					case 2:	// Erase entire screen (Most BBS terminals also move to 1/1)
						this.graphic.clear();
						break;
				}
				break;
			case 'K':	// Erase in line
				param_defaults(p, [0]);
				switch(p[0]) {
					case 0:	// Erase to eol
						for (x = this.pos.x; x<this.pos.width; x++) {
							this.graphic.data[x][this.pos.y].ch = this.graphic.ch;
							this.graphic.data[x][this.pos.y].attr = this.attr.value;
						}
						break;
					case 1:	// Erase to start of line
						for (x = 0; x<=this.pos.x; x++) {
							this.graphic.data[x][this.pos.y].ch = this.graphic.ch;
							this.graphic.data[x][this.pos.y].attr = this.attr.value;
						}
						break;
					case 2:	// Erase entire line
						for (x = 0; x<this.graphic.width; x++) {
							this.graphic.data[x][this.pos.y].ch = this.graphic.ch;
							this.graphic.data[x][this.pos.y].attr = this.attr.value;
						}
						break;
					default:
						break;
				}
				break;
			case 'P':	// Delete character
				param_defaults(p, [1]);
				if (p[1] > this.graphic.width - this.pos.x)
					p[1] = this.graphic.width - this.pos.x;
				if (this.pos.x < this.graphic.width-1)
					this.graphic.copy(this.pos.x + p[1], this.pos.y, this.graphic.width - 1, this.pos.y, this.pos.x, this.pos.y);
				for (x = 0; x<p[1]; x++) {
					this.graphic.data[(this.width - 1) - x][this.pos.y].ch = this.graphic.ch;
					this.graphic.data[(this.width - 1) - x][this.pos.y].attr = this.attr.value;
				}
				break;
			case 'X':	// Erase character
				param_defaults(p, [1]);
				if (p[1] > this.graphic.width - this.pos.x)
					p[1] = this.graphic.width - this.pos.x;
				for (x = 0; x<p[1]; x++) {
					this.graphic.data[this.pos.x + x][this.pos.y].ch = this.graphic.ch;
					this.graphic.data[this.pos.x + x][this.pos.y].attr = this.attr.value;
				}
				break;
			case 'm':
				param_defaults(p, [0]);
				for (i=0; i<p.length; i++) {
					switch(p[i]) {
						case 0:
							this.attr.value = this.graphic.attribute.value;
							break;
						case 1:
							this.attr.bright = true;
							break;
						case 2:
						case 22:
							this.attr.bright = false;
							break;
						case 5:
						case 6:
							this.attr.blink = true;
							break;
						case 7:
							tg = this.attr.bg;
							this.attr.bg = this.attr.fg;
							this.attr.fg = tg;
							break;
						case 8:
							this.attr.fg = this.attr.bg;
							break;
						case 25:
							this.attr.blink = false;
							break;
						case 30:
							this.attr.fg = Attribute.BLACK;
							break;
						case 31:
							this.attr.fg = Attribute.RED;
							break;
						case 32:
							this.attr.fg = Attribute.GREEN;
							break;
						case 33:
							this.attr.fg = Attribute.YELLOW;
							break;
						case 34:
							this.attr.fg = Attribute.BLUE;
							break;
						case 35:
							this.attr.fg = Attribute.MAGENTA;
							break;
						case 36:
							this.attr.fg = Attribute.CYAN;
							break;
						case 37:
							this.attr.fg = Attribute.WHITE;
							break;
						case 40:
							this.attr.bg = Attribute.BLACK;
							break;
						case 41:
							this.attr.bg = Attribute.RED;
							break;
						case 42:
							this.attr.bg = Attribute.GREEN;
							break;
						case 43:
							this.attr.bg = Attribute.YELLOW;
							break;
						case 44:
							this.attr.bg = Attribute.BLUE;
							break;
						case 45:
							this.attr.bg = Attribute.MAGENTA;
							break;
						case 46:
							this.attr.bg = Attribute.CYAN;
							break;
						case 47:
							this.attr.bg = Attribute.WHITE;
							break;
					}
				}
				break;
			case 's':
				this.saved_pos.x = this.pos.x;
				this.saved_pos.y = this.pos.y;
				break;
			case 'u':
				this.pos.x = this.saved_pos.x;
				this.pos.y = this.saved_pos.y;
				break;
			// Still TODO...
			case 'n':	// Device status report... no action from this object.
			case 'Z':	// Back tabulate
			case 'S':	// Scroll up
			case 'T':	// Scroll down
			case 'L':	// Insert line
			case 'M':	// Delete line (also ANSI music!)
			default:
				log("Sent unsupported ANSI sequence '"+ext+pb+ib+fb+"' please let shurd@sasktel.net net know about this so it can be fixed.");
		}
	}
	if ((m=str.match(Screen.ANSIFragRe)) !== null) {
		str = m[1];
		this.escbuf = m[2];
	}
	// Send regular chars before the sequence...
	for (i=0; i<str.length; i++)
		writech(this, str[i]);

};
