/*
 * Screen object...
 * This object is the estimate of what a screen looks like modeled
 * as a Graphic object (Screen.graphic).
 * 
 * The Graphic object should not be written directly, but instead,
 * all data should be provided using the print() method.
 */

require('graphic.js', 'Graphic');
require('attribute.js', 'Attribute');

function Screen(w, h, attr, fill, puttext)
{
	'use strict';
	this.graphic = new Graphic(w, h, attr, fill, puttext);
	this.escbuf = '';
	this.pos = {x:0, y:0};
	this.saved_pos = {x:0, y:0};
	this.attr = new Attribute(7);
	this.new_lines = 0;
	//this.touched = [];
	this.full = false;
}
Screen.ANSIRe = /^\x1b\[([<-?]{0,1})([0-;]*)([\u0020-\/]*)([@-~])/;
Screen.ANSIFragRe = /^\x1b(\[([<-?]{0,1})([0-;]*)([\u0020-\/]*)([@-~])?)?$/;
Screen.StripColonRe = /:[^;:]*/g;

Screen.prototype.print=function(str) {
	'use strict';
	var m;
	var ext;
	var pb;
	var ib;
	var fb;
	var p;
	var i;
	var tg;
	var x;
	var y;
	var tb;

	function writech(scr, ch) {
		function check_scrollup(scr) {
			while (scr.pos.y >= scr.graphic.height) {
				scr.graphic.scrollup();
				scr.pos.y -= 1;
				scr.full = true;
				if (scr.touched !== undefined) {
					scr.touched = [];
				}
			}
		}

		// Handle special chars.
		switch(ch) {
			case '\x00':	// NUL is not displayed.
				break;
			case '\x07':	// Beep is not displayed.
				break;
			case '\x08':	// Backspace.
				scr.pos.x -= 1;
				if (scr.pos.x < 0) {
					scr.pos.x = 0;
				}
				break;
			case '\x09':	// Tab
				do {
					scr.pos.x += 1;
				} while((scr.pos.x % 8) !== 0);
				if (scr.pos.x >= scr.graphic.width) {
					scr.pos.x = scr.graphic.width-1;
				}
				break;
			case '\x0a':	// Linefeed
				scr.pos.y += 1;
				scr.new_lines += 1;
				check_scrollup(scr);
				break;
			case '\x0c':	// Form feed (clear screen and home)
				scr.graphic.clear();
				scr.pos.x=0;
				scr.pos.y=0;
				scr.new_lines=0;
				scr.full = true;
				if (scr.touched !== undefined) {
					scr.touched = [];
				}
				break;
			case '\x0d':	// Carriage return
				scr.pos.x = 0;
				break;
			default:
				scr.graphic.setCell(ch, scr.attr.value, scr.pos.x, scr.pos.y);
				if (scr.touched !== undefined) {
					scr.touched.push({sx:scr.pos.x, sy:scr.pos.y, ex:scr.pos.x, ey:scr.pos.y, b:[ascii(ch), scr.attr.value]});
				}
				scr.pos.x += 1;
				if (scr.pos.x >= scr.graphic.width) {
					scr.pos.x = 0;
					scr.pos.y += 1;
					scr.new_lines += 1;
					check_scrollup(scr);
				}
		}
	}

	function param_defaults(params, defaults) {
		var pdi;

		for (pdi=0; pdi<defaults.length; pdi += 1) {
			if (params[pdi] === undefined || params[pdi].length === 0) {
				params[pdi]=defaults[pdi];
			}
		}
		for (pdi=0; pdi<params.length; pdi += 1) {
			if (params[pdi]===undefined || params[pdi]==='') {
				params[pdi] = 0;
			}
			else {
				params[pdi] = parseInt(params[pdi], 10);
			}
		}
	}

	// Prepend the ESC buffer to avoid failing on split ANSI
	str = this.escbuf + str;
	this.escbuf = '';

	while(str.length > 0) {
		if (str[0] !== '\x1b') {
			writech(this, str[0]);
			str = str.substr(1);
		}
		else {
			m = str.match(Screen.ANSIRe);
			if (m === null) {
				m=str.match(Screen.ANSIFragRe);
				if (m !== null) {
					this.escbuf = m[0];
					str = '';
					break;
				}
				writech(this, str[0]);
				str = str.substr(1);
			}
			else {
				ext = m[1];
				pb = m[2];
				ib = m[3];
				fb = m[4];

				str = str.substr(m[0].length);

				// We don't support any : paramters... strip and ignore.
				p = pb.replace(Screen.StripColonRe, '');
				p = p.split(';');

				switch(fb) {
					case '@':	// Insert character
						param_defaults(p, [1]);
						if (p[1] > this.graphic.width - this.pos.x) {
							p[1] = this.graphic.width - this.pos.x;
						}
						if (this.pos.x < this.graphic.width-1) {
							this.graphic.copy(this.pos.x, this.pos.y, this.graphic.width-1 - p[1], this.pos.y, this.pos.x + p[1], this.pos.y);
						}
						for (x = 0; x<p[1]; x += 1) {
							this.graphic.setCell(this.graphic.ch, this.attr.value, this.pos.x + x, this.pos.y);
						}
						if (this.touched !== undefined) {
							tb = {sx:this.pos.x, sy:this.pos.y, ex:this.graphic.width-1, ey:this.pos.y, b:this.graphic.puttext.slice((this.pos.y * this.graphic.width + this.pos.x) * 2, (this.pos.y + 1) * this.graphic.width * 2)};
							this.touched.push(tb);
						}
						break;
					case 'A':	// Cursor Up
						param_defaults(p, [1]);
						this.pos.y -= p[0];
						if (this.pos.y < 0) {
							this.pos.y = 0;
						}
						// TODO
						//if (this.touched.length > 0 || this.full)
						this.new_lines = this.pos.y+1;
						break;
					case 'B':	// Cursor Down
						param_defaults(p, [1]);
						this.pos.y += p[0];
						if (this.pos.y >= this.graphic.height) {
							this.pos.y = this.graphic.height-1;
						}
						// TODO
						//if (this.touched.length > 0 || this.full)
						this.new_lines = this.pos.y+1;
						break;
					case 'C':	// Cursor Right
						param_defaults(p, [1]);
						this.pos.x += p[0];
						if (this.pos.x >= this.graphic.width) {
							this.pos.x = this.graphic.width-1;
						}
						break;
					case 'D':	// Cursor Left
						param_defaults(p, [1]);
						this.pos.x -= p[0];
						if (this.pos.x < 0) {
							this.pos.x = 0;
						}
						break;
					case 'H':	// Cursor position
					case 'f':
						param_defaults(p, [1,1]);
						if (p[0] >= 1 && p[0] < this.graphic.height && p[1] >= 1 && p[1] <= this.graphic.width) {
							this.pos.x = p[1]-1;
							this.pos.y = p[0]-1;
							// TODO
							//if (this.touched.length > 0 || this.full)
							this.new_lines = p[0];
						}
						break;
					case 'J':	// Erase in screen
						param_defaults(p, [0]);
						switch(p[0]) {
							case 0:	// Erase to end of screen...
								for (x = this.pos.x; x<this.pos.width; x += 1) {
									this.graphic.setCell(this.graphic.ch, this.attr.value, x, this.pos.y);
								}
								for (y = this.pos.y+1; y<this.pos.height; y += 1) {
									for (x = 0; x<this.graphic.width; x += 1) {
										this.graphic.setCell(this.graphic.ch, this.attr.value, x, y);
									}
								}
								this.full = true;
								if (this.touched !== undefined) {
									this.touched = [];
								}
								break;
							case 1:	// Erase to beginning of screen...
								for (y = 0; y < this.pos.y; y += 1) {
									for (x = 0; x<this.graphic.width; x += 1) {
										this.graphic.setCell(this.graphic.ch, this.attr.value, x, y);
									}
								}
								for (x = 0; x<=this.pos.x; x += 1) {
									this.graphic.setCell(this.graphic.ch, this.attr.value, x, this.pos.y);
								}
								this.full = true;
								if (this.touched !== undefined) {
									this.touched = [];
								}
								break;
							case 2:	// Erase entire screen (Most BBS terminals also move to 1/1)
								this.graphic.clear();
								this.new_lines = 0;
								this.full = true;
								if (this.touched !== undefined) {
									this.touched = [];
								}
								break;
						}
						break;
					case 'K':	// Erase in line
						param_defaults(p, [0]);
						switch(p[0]) {
							case 0:	// Erase to eol
								for (x = this.pos.x; x<this.graphic.width; x += 1) {
									this.graphic.setCell(this.graphic.ch, this.attr.value, x, this.pos.y);
								}
								if (this.touched !== undefined) {
									tb = {sx:this.pos.x, sy:this.pos.y, ex:this.graphic.width-1, ey:this.pos.y, b:this.graphic.puttext.slice((this.pos.y * this.graphic.width + this.pos.x) * 2, (this.pos.y + 1) * this.graphic.width * 2)};
									this.touched.push(tb);
								}
								break;
							case 1:	// Erase to start of line
								for (x = 0; x<=this.pos.x; x += 1) {
									this.graphic.setCell(this.graphic.ch, this.attr.value, x, this.pos.y);
								}
								if (this.touched !== undefined) {
									tb = {sx:0, sy:this.pos.y, ex:this.pos.x, ey:this.pos.y, b:this.graphic.puttext.slice(this.pos.y * this.graphic.width, (this.pos.y * this.graphic.width + this.pos.x) * 2)};
									this.touched.push(tb);
								}
								break;
							case 2:	// Erase entire line
								for (x = 0; x<this.graphic.width; x += 1) {
									this.graphic.setCell(this.graphic.ch, this.attr.value, x, this.pos.y);
								}
								if (this.touched !== undefined) {
									tb = {sx:this.pos.x, sy:this.pos.y, ex:this.graphic.width-1, ey:this.pos.y, b:this.graphic.puttext.slice(this.pos.y * this.graphic.width * 2, (this.pos.y + 1) * this.graphic.width * 2)};
									this.touched.push(tb);
								}
								break;
						}
						break;
					case 'P':	// Delete character
						param_defaults(p, [1]);
						if (p[1] > this.graphic.width - this.pos.x) {
							p[1] = this.graphic.width - this.pos.x;
						}
						if (this.pos.x < this.graphic.width-1) {
							this.graphic.copy(this.pos.x + p[1], this.pos.y, this.graphic.width - 1, this.pos.y, this.pos.x, this.pos.y);
						}
						for (x = 0; x<p[1]; x += 1) {
							this.graphic.setCell(this.graphic.ch, this.attr.value, (this.width - 1) - x, this.pos.y);
						}
						if (this.touched !== undefined) {
							tb = {sx:this.pos.x, sy:this.pos.y, ex:this.graphic.width-1, ey:this.pos.y, b:this.graphic.puttext.slice((this.pos.y * this.graphic.width + this.pos.x) * 2, (this.pos.y + 1) * this.graphic.width * 2)};
							this.touched.push(tb);
						}
						break;
					case 'X':	// Erase character
						param_defaults(p, [1]);
						if (p[1] > this.graphic.width - this.pos.x) {
							p[1] = this.graphic.width - this.pos.x;
						}
						for (x = 0; x<p[1]; x += 1) {
							this.graphic.setCell(this.graphic.ch, this.attr.value, this.pos.x + x, this.pos.y);
						}
						if (this.touched !== undefined) {
							tb = {sx:this.pos.x, sy:this.pos.y, ex:this.pos.x + p[1], ey:this.pos.y, b:this.graphic.puttext.slice((this.pos.y * this.graphic.width + this.pos.x) * 2, (this.pos.y * this.graphic.width + this.pos.x + p[1]) * 2)};
							this.touched.push(tb);
						}
						break;
					case 'm':
						param_defaults(p, [0]);
						for (i=0; i<p.length; i += 1) {
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
						// TODO
						//if (this.touched.length > 0 || this.full)
						this.new_lines = this.pos.y;
						break;
					// Still TODO...
					//case 'n':	// Device status report... no action from this object.
					//case 'Z':	// Back tabulate
					//case 'S':	// Scroll up
					//case 'T':	// Scroll down
					//case 'L':	// Insert line
					//case 'M':	// Delete line (also ANSI music!)
					default:
						log("Sent unsupported ANSI sequence '"+ext+pb+ib+fb+"' please let shurd@sasktel.net net know about this so it can be fixed.");
				}
			}
		}
	}
};
