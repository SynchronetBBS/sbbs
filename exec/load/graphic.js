/*
 * "Graphic" object
 * Allows a graphic to be stored in memory and portions of it redrawn on command
 * @format.tab-size 4, @format.use-tabs true
 */

require("userdefs.js", "USER_ICE_COLOR");

/*
 * Class that represents a drawable object.
 * w = width (default of 80)
 * h = height (default of 24)
 * attr = default attribute (default of 7 ie: LIGHTGRAY)
 * attr_mask = mask attribute bits (e.g. to disable BLINK)
 * ch = default character (default of space)
 *
 * Instance variable data contains an array of array of Graphics.Cell objects
 *
 * Instance variables atcodes are slated for removal.
 */
function Graphic(w,h,attr,ch, dw)
{
	if(ch==undefined)
		this.ch=' ';
	else
		this.ch=ch;

	if(attr==undefined)
		this.attribute=7;
	else
		this.attribute=attr;

	if(h==undefined)
		this.height=24;
	else
		this.height=h;

	if(w==undefined)
		this.width=80;
	else
		this.width=w;

	if (dw !== undefined)
		this.doorway_mode = dw;

	this.atcodes=true;
	this.cpm_eof=true;
	this.attr_mask=0xff;
	this.ansi_crlf=true;
	this.illegal_characters = [0, 7, 8, 9, 10, 12, 13, 27];
	this.autowrap=true;
	this.revision="$Revision: 1.84 $".split(' ')[1];

	this.data=new Array(this.width);
	for(var y=0; y<this.height; y++) {
		for(var x=0; x<this.width; x++) {
			if(y==0) {
				this.data[x]=new Array(this.height);
			}
			this.data[x][y]=new this.Cell(this.ch,this.attribute);
		}
	}
}

Graphic.prototype.getattr = function(a) {
	// Translates blink to bright BG
	if (console.term_supports(USER_ICE_COLOR) && (a & 0x80)) {
		a &= ~0x80;
		a |= 0x400;
	}
	return a;
}

Graphic.prototype.extend = function() {
	for(var x = 0; x < this.width; x++)
		this.data[x][this.height] = new this.Cell(this.ch,this.attribute);
	this.height++;
}

/*
 * Default to not DoorWay mode, allow it to be overridden per object, or
 * globally.
 */
Graphic.prototype.doorway_mode = false;

/*
 * Load sbbsdefs.js into Graphic.defs
 */
Graphic.prototype.defs = {};
load(Graphic.prototype.defs, "cga_defs.js");

/*
 * Load ansiterm_lib into Graphic.ansi
 */
Graphic.prototype.ansi = {};
load(Graphic.prototype.ansi, "ansiterm_lib.js");

/*
 * The Cell subclass which contains the attribute and character for a
 * single cell.
 */
Graphic.prototype.Cell = function(ch,attr)
{
	this.ch=ch;
	this.attr=attr;
};

/*
 * BIN property is the string representation of the graphic in a series of
 * char/attr pairs.
 */
Object.defineProperty(Graphic.prototype, "BIN", {
	get: function() {
		var bin = '';
		var x;
		var y;

		for (y=0; y<this.height; y++) {
			for (x=0; x<this.width; x++) {
				bin += this.data[x][y].ch;
				bin += ascii(this.data[x][y].attr);
			}
		}
		return bin;
	},
	set: function(bin) {
		var x;
		var y;
		var pos = 0;
		var blen = bin.length;

		for (y=0; y<this.height; y++) {
			for (x=0; x<this.width; x++) {
				if (blen < pos+2)
					return;
				var char = bin.charAt(pos);
				this.data[x][y] = new this.Cell(char, bin.charCodeAt(pos+1));
				pos += 2;
			}
		}
	}
});

/*
 * ANSI property is the string representation of the graphic as ANSI
 * 
 * NOTE: Reading the ANSI property generates ANSI sequences which can put
 *       printable characters in the last column of the display (e.g. col 80)
 *       potentially resulting in extra blank lines in some terminals.
 *       To change this behavior, decrement the 'width' property first.
 */
Object.defineProperty(Graphic.prototype, "ANSI", {
	get: function() {
		var x;
		var y;
    	var lines=[];
    	var curattr=7;
		var ansi = '';
		var char;

		for(y=0; y<this.height; y++) {
			for(x=0; x<this.width; x++) {
				ansi += this.ansi.attr(this.data[x][y].attr, curattr);
            	curattr = this.data[x][y].attr;
				/* Replace illegal characters with default char */
				if (this.illegal_characters.indexOf(ascii(this.data[x][y].ch)) >= 0) {
					if (this.doorway_mode)
						char = ascii(0) + this.data[x][y].ch;
					else
						char = this.ch;
				}
				else
            		char = this.data[x][y].ch;
            	/* Don't put printable chars in the last column */
//            	if(char == ' ' || (x<this.width-1))
                	ansi += char;
        	}
			if(this.ansi_crlf)
				ansi += '\r\n';
			else {
				ansi += this.ansi.cursor_position.move('down');
				ansi += this.ansi.cursor_position.move('left', this.width);
			}
    	}
    	return ansi;
	},
	set: function(ans) {
		var attr = this.attribute;
		var saved = {};
		var x = 0;
		var y = 0;
		var std_cmds = {
			'm':function(params, obj) {
				var bg = attr & obj.defs.BG_LIGHTGRAY;
				var fg = attr & obj.defs.LIGHTGRAY;
				var hi = attr & obj.defs.HIGH;
				var bnk = attr & obj.defs.BLINK;

				if (params[0] === undefined || params[0] === '')
					params[0] = 0;

				while (params.length) {
					switch (parseInt(params[0], 10)) {
						case 0:
							bg = 0;
							fg = obj.defs.LIGHTGRAY;
							hi = 0;
							bnk = 0;
							break;
						case 1:
							hi = obj.defs.HIGH;
							break;
						case 5:
							bnk = obj.defs.BLINK;
							break;
						case 40:
							bg = 0;
							break;
						case 41:
							bg = obj.defs.BG_RED;
							break;
						case 42: 
							bg = obj.defs.BG_GREEN;
							break;
						case 43:
							bg = obj.defs.BG_BROWN;
							break;
						case 44:
							bg = obj.defs.BG_BLUE;
							break;
						case 45:
							bg = obj.defs.BG_MAGENTA;
							break;
						case 46:
							bg = obj.defs.BG_CYAN;
							break;
						case 47:
							bg = obj.defs.BG_LIGHTGRAY;
							break;
						case 30:
							fg = obj.defs.BLACK;
							break;
						case 31:
							fg = obj.defs.RED;
							break;
						case 32:
							fg = obj.defs.GREEN;
							break;
						case 33:
							fg = obj.defs.BROWN;
							break;
						case 34:
							fg = obj.defs.BLUE;
							break;
						case 35:
							fg = obj.defs.MAGENTA;
							break;
						case 36:
							fg = obj.defs.CYAN;
							break;
						case 37:
							fg = obj.defs.LIGHTGRAY;
							break;
					}
					params.shift();
				}
				attr = bg + fg + hi + bnk;
			},
			'H':function(params, obj) {
				if (params[0] === undefined || params[0] === '')
					params[0] = 1;
				if (params[1] === undefined || params[1] === '')
					params[1] = 1;

				y = parseInt(params[0], 10) - 1;
				if (y < 0)
					y = 0;
				if (y >= obj.height)
					y = obj.height-1;

				x = parseInt(params[1], 10) - 1;
				if (x < 0)
					x = 0;
				if (x >= obj.width)
					x = obj.width-1;
			},
			'A':function(params) {
				if (params[0] === undefined || params[0] === '')
					params[0] = 1;
	
				y -= parseInt(params[0], 10);
				if (y < 0)
					y = 0;
			},
			'B':function(params, obj) {
				if (params[0] === undefined || params[0] === '')
					params[0] = 1;

				y += parseInt(params[0], 10);
				if (y >= obj.height)
					y = obj.height-1;
			},
			'C':function(params, obj) {
				if (params[0] === undefined || params[0] === '')
					params[0] = 1;

				x += parseInt(params[0], 10);
				if (x >= obj.width)
					x = obj.width - 1;
			},
			'D':function(params) {
				if (params[0] === undefined || params[0] === '')
					params[0] = 1;

				x -= parseInt(params[0], 10);
				if (x < 0)
					x = 0;
			},
			'J':function(params,obj) {
				if (params[0] === undefined || params[0] === '')
					params[0] = 0;

				if (parseInt(params[0], 10) == 2)
					obj.clear();
			},
			's':function(params) {
				saved={'x':x, 'y':y};
			},
			'u':function(params) {
				if (saved.x !== undefined)
					x = saved.x;
				if (saved.y !== undefined)
					y = saved.y;
			}
		};
		std_cmds.f = std_cmds.H;
		var line;
		var m;
		var paramstr;
		var cmd;
		var params;
		var ch;
		var doorway_char = false;

		/* parse 'ATCODES'?? 
		ans = ans.replace(/@(.*)@/g,
			function (str, code, offset, s) {
				return bbs.atcode(code);
			}
		);
		*/
		while(ans.length > 0) {
			if (!doorway_char) {
				m = ans.match(/^\x1b\[([\x30-\x3f]*)([\x20-\x2f]*[\x40-\x7e])/);
				if (m !== null) {
					paramstr = m[1];
					cmd = m[2];
					if (paramstr.search(/^[<=>?]/) != 0) {
						params=paramstr.split(/;/);
	
						if (std_cmds[cmd] !== undefined)
							std_cmds[cmd](params,this);
					}
					ans = ans.substr(m[0].length);
   		            continue;
				}
				if(this.cpm_eof == true && ans[0] == '\x1a') // EOF
					break;
			}

			/* set character and attribute */
			ch = ans[0];
			ans = ans.substr(1);

	        /* Handle non-ANSI cursor positioning control characters */
			switch(ch) {
                case '\r':
                    x=0;
                    break;
                case '\n':
                    y++;
                    break;
                case '\t':
                    x += 8-(x%8);
                    break;
                case '\b':
                    if(x > 0)
						x--;
            		break;
				case '\x00':
					if (this.doorway_mode) {
						if (!doorway_char) {
							doorway_char = true;
							break;
						}
					}
                default:
			        this.data[x][y]=new this.Cell(ch,attr);
			        x++;
					doorway_char = false;
                    break;
            }
			/* validate position/scroll */
			if (x < 0)
				x = 0;
			if (y < 0)
				y = 0;
			if(x>=this.width) {
				x=0;
				y++;
			}
			while(y >= this.height) {
				if(this.auto_extend === true)
					this.extend();
				else {
					this.scroll();
					y--;
				}
			}
		}
	}
});

// return ctrl-a codes to obtain a certain attribute
Graphic.prototype.ctrl_a_sequence = function(attr, curattr)
{
	if(attr == curattr)
		return '';
	var str = '';
	if(((curattr&this.defs.BLINK) && !(attr&this.defs.BLINK)) 
		|| ((curattr&this.defs.HIGH) && !(attr&this.defs.HIGH))) {
		str += "\x01N";
		curattr = this.defs.LIGHTGRAY;
	}
	if((attr&0xf0) != (curattr&0xf0)) {
		if((attr&this.defs.BLINK) && !(curattr&this.defs.BLINK))
			str += "\x01I";
		switch(attr&0x70) {
			case 0:
				str += "\x01" + "0";
				break;
			case this.defs.BG_RED:
				str += "\x01" + "1";
				break;
			case this.defs.BG_GREEN:
				str += "\x01" + "2";
				break;
			case this.defs.BG_BROWN:
				str += "\x01" + "3";
				break;
			case this.defs.BG_BLUE:
				str += "\x01" + "4";
				break;
			case this.defs.BG_MAGENTA:
				str += "\x01" + "5";
				break;
			case this.defs.BG_CYAN:
				str += "\x01" + "6";
				break;
			case this.defs.BG_LIGHTGRAY:
				str += "\x01" + "7";
				break;
		}
	}
	if((attr&0x0f) != (curattr&0x0f)) {
		if((attr&this.defs.HIGH) && !(curattr&this.defs.HIGH))
			str += "\x01H";
		switch(attr&0x07) {
			case this.defs.BLACK:
				str += "\x01K";
				break;
			case this.defs.RED:
				str += "\x01R";
				break;
			case this.defs.GREEN:
				str += "\x01G";
				break;
			case this.defs.BROWN:
				str += "\x01Y";
				break;
			case this.defs.BLUE:
				str += "\x01B";
				break;
			case this.defs.MAGENTA:
				str += "\x01M";
				break;
			case this.defs.CYAN:
				str += "\x01C";
				break;
			case this.defs.LIGHTGRAY:
				str += "\x01W";
				break;
		}
	}
	return str;
}	

Object.defineProperty(Graphic.prototype, "MSG", {
	get: function() {
		var x;
		var y;
    	var lines=[];
    	var curattr = this.defs.LIGHTGRAY;
		var msg = '';
		var ch;

		for(y=0; y<this.height; y++) {
			for(x=0; x<this.width; x++) {
				msg += this.ctrl_a_sequence(this.data[x][y].attr, curattr);
            	curattr = this.data[x][y].attr;
            	ch = this.data[x][y].ch;
				if(ch == '\x01')	// Convert a Ctrl-A char to a Ctrl-AA (escaped)
					ch += 'A';
				else if (this.illegal_characters.indexOf(ascii(ch)) >= 0) {
					if (this.doorway_mode)
						ch = ascii(0) + ch;
					else
						ch=this.ch;
				}
                msg += ch;
        	}
			msg += '\x01N\r\n';
			curattr = this.defs.LIGHTGRAY;
    	}
    	return msg;
	},
	set: function(msg) {
		this.putmsg(undefined,undefined,msg,true);
	}
});


/*
 * Returns an HTML encoding of the graphic data.
 *
 * TODO: get rid of the ANSI phase as unnecessary
 */
Object.defineProperty(Graphic.prototype, "HTML", {
	get: function() {
        return html_encode(this.ANSI, /* ex-ascii: */true, /* whitespace: */false, /* ansi: */true, /* Ctrl-A: */false);
    }
});

/*
 * Resets the graphic to all this.ch/this.attr Cells
 */
Graphic.prototype.clear = function()
{
	this.data=new Array(this.width);
	for(var y=0; y<this.height; y++) {
		for(var x=0; x<this.width; x++) {
			if(y==0) {
				this.data[x]=new Array(this.height);
			}
			this.data[x][y]=new this.Cell(this.ch,this.attribute);
		}
	}
};

/*
 * Draws the graphic using the console object optionally delaying between
 * characters.
 *
 * TODO: Remove optional delay.  If the functionality is needed, maybe add
 *       a callback.
 */
Graphic.prototype.draw = function(xpos,ypos,width,height,xoff,yoff,delay)
{
	var x;
	var y;

	if(width==undefined)
		width=this.width;
	if(height==undefined)
		height=this.height;
	if(xoff==undefined)
		xoff=0;
	if(yoff==undefined)
		yoff=0;
	if(xpos == 'center')	// center
		xpos = Math.floor((console.screen_columns - width) / 2) + 1;
	if(xpos==undefined || xpos < 1)
		xpos=1;
	if(ypos == 'center')	// center
		ypos = Math.ceil((console.screen_rows - height) / 2) + 1;
	if(ypos==undefined || ypos < 1)
		ypos=1;
	if(delay==undefined)
		delay=0;
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false);
	}
	if(xpos+width-1 > console.screen_columns || ypos+height-1 > console.screen_rows) {
		log(LOG_DEBUG, "Attempt to draw outside of screen: " + (xpos+width-1) + "x" + (ypos+height-1)
			+ format(" > %ux%u", console.screen_columns, console.screen_rows));
		return(false);
	}
	for(y=0;y<height; y++) {
		console.gotoxy(xpos,ypos+y);
		for(x=0; x<width; x++) {
			// Do not draw to the bottom right corner of the screen-would scroll
			if(this.autowrap == false
				|| xpos+x != console.screen_columns
				|| ypos+y != console.screen_rows) {
				console.attributes=this.getattr(this.data[x+xoff][y+yoff].attr & this.attr_mask);
				var ch=this.data[x+xoff][y+yoff].ch;
				if (this.illegal_characters.indexOf(ascii(ch)) >= 0) {
					if (this.doorway_mode)
						ch = ascii(0) + ch;
					else
						ch=this.ch;
				}
				console.write(ch);
			}
		}
		if(delay)
			mswait(delay);
	}
	return(true);
};

/*
 * Does a random draw of the graphic...
 * Maybe this should stay, maybe not.  TODO: Decide.
 */
Graphic.prototype.drawfx = function(xpos,ypos,width,height,xoff,yoff)
{
	var x;
	var y;

	if(xpos==undefined)
		xpos=1;
	if(ypos==undefined)
		ypos=1;
	if(width==undefined)
		width=this.width;
	if(height==undefined)
		height=this.height;
	if(xoff==undefined)
		xoff=0;
	if(yoff==undefined)
		yoff=0;
	if(xpos == 'center')	// center
		xpos = Math.floor((console.screen_columns - width) / 2) + 1;
	if(ypos == 'center')	// center
		ypos = Math.ceil((console.screen_rows - height) / 2) + 1;
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false);
	}
	if(xpos+width-1 > console.screen_columns || ypos+height-1 > console.screen_rows) {
		alert("Attempt to draw outside of screen");
		return(false);
	}
	var placeholder=new Array(width);
	for(x=0;x<width;x++) {
		placeholder[x]=new Array(height);
		for(y=0;y<placeholder[x].length;y++) {
			placeholder[x][y]={'x':xoff+x,'y':yoff+y};
		}
	}
	var count=0;
	var interval=10;
	while(placeholder.length) {
		count++;
		if(count == interval) {
			count=0;
			mswait(15);
		}
		var randx=random(placeholder.length);
		if(!placeholder[randx] || !placeholder[randx].length) {
			placeholder.splice(randx,1);
			continue;
		}
		var randy=random(placeholder[randx].length);
		var position=placeholder[randx][randy];
		if(!position) 
			continue;
		if(position.x != console.screen_columns	|| position.y != console.screen_rows) {
			if(xpos+position.x >= console.screen_columns && ypos+position.y >= console.screen_rows) {
				placeholder[randx].splice(randy,1);
				continue;
			}
			console.gotoxy(xpos+position.x,ypos+position.y);
			console.attributes=this.getattr(this.data[position.x][position.y].attr & this.attr_mask);
			var ch=this.data[position.x][position.y].ch;
			if (this.illegal_characters.indexOf(ascii(ch)) >= 0) {
				if (this.doorway_mode)
					ch = ascii(0) + ch;
				else
					ch=this.ch;
			}
			console.write(ch);
		}
		placeholder[randx].splice(randy,1);
	}
	console.home();
	return(true);
};

/*
 * Loads a ANS or BIN file.
 * TODO: The ASC load is pretty much guaranteed to be broken.
 */
Graphic.prototype.load = function(filename, offset)
{
	var file_type=file_getext(filename);
	var f=new File(filename);
	var ch;
	var attr;
	var l;

	if(!file_type)
		throw new Error("unsupported file type: " + filename);

	switch(file_type.substr(1).toUpperCase()) {
	case "ANS":
		if(!(f.open("rb",true)))
			return(false);
		this.ANSI = f.read();
		f.close();
		break;
	case "BIN":
		if(!(f.open("rb",true)))
			return(false);
		if(offset)
			f.position = offset * this.height * this.width * 2;
		this.BIN = f.read(this.height * this.width * 2);
		f.close();
		break;
	case "ASC":
		if(!(f.open("r",true,4096)))
			return(false);
		var lines=f.readAll();
		f.close();
		for (l in lines)
			this.putmsg(undefined,undefined,l,true);
		break;
	default:
		throw new Error("unsupported file type: " + filename);
	}
	return(true);
};

/*
 * The inverse of load()... saves a BIN representation to a file.
 */
Graphic.prototype.save = function(filename)
{
	var x;
	var y;
	var f=new File(filename);

	if(!(f.open("wb",true)))
		return(false);
	f.write(this.BIN);
	f.close();
	return(true);
};

/*
 * Moves the image in the graphic.
 * Scrolling up by one is needed for the putmsg implementation.
 *
 * TODO: Doesn't belong here, but lbshell.js uses it.
 */
Graphic.prototype.scroll = function(count)
{
	var x;
	var i;

	if (count === undefined)
		count = 1;
	if (count < 0)
		return false;
	for (x=0; x<this.width; x++) {
		for (i=0; i<count; i++) {
			this.data[x].shift();
			this.data[x].push(new this.Cell(this.ch, this.attribute));
		}
	}
	return true;
};

/*
 * Emulates console.putmsg() into a graphic object.
 * Supports @-codes.
 * Returns the number of times the graphic was scrolled.
 *
 * TODO: Doesn't belong here, but lbshell.js uses it.
 */
Graphic.prototype.putmsg = function(xpos, ypos, txt, attr, scroll)
{
	var curattr=attr;
	var ch;
	var x=xpos?xpos-1:0;
	var y=ypos?ypos-1:0;
	var p=0;
	var scrolls=0;

	if(curattr==undefined)
		curattr=this.attribute;
	/* Expand @-codes */
	if(txt==undefined || txt==null || txt.length==0) {
		return(0);
	}
	if(this.atcodes) {
		txt=txt.toString().replace(/@(.*)@/g,
			function (str, code, offset, s) {
				return bbs.atcode(code);
			}
		);
	}
	
	/* wrap text at graphic width */
	txt=word_wrap(txt,this.width);
	
	/* ToDo: Expand \1D, \1T, \1<, \1Z */
	/* ToDo: "Expand" (ie: remove from string when appropriate) per-level/per-flag stuff */
	/* ToDo: Strip ANSI (I betcha @-codes can slap it in there) */
	while(p<txt.length) {
		while(y>=this.height) {
			if(!scroll) {
				alert("Attempt to draw outside of screen");
				return false;
			}
			scrolls++;
			this.scroll();
			y--;
		}
		
		ch=txt[p++];
		switch(ch) {
			case '\x01':		/* CTRL-A code */
				ch=txt[p++].toUpperCase();
				switch(ch) {
					case '\x01':	/* A "real" ^A code */
						this.data[x][y].ch=ch;
						this.data[x][y].attr=curattr;
						x++;
						if(x>=this.width) {
							x=0;
							y++;
							log("next char: [" + txt[p] + "]");
							if(txt[p] == '\r') p++;
							if(txt[p] == '\n') p++;
						}
						break;
					case 'K':	/* Black */
						curattr=(curattr)&0xf8;
						break;
					case 'R':	/* Red */
						curattr=((curattr)&0xf8)|this.defs.RED;
						break;
					case 'G':	/* Green */
						curattr=((curattr)&0xf8)|this.defs.GREEN;
						break;
					case 'Y':	/* Yellow */
						curattr=((curattr)&0xf8)|this.defs.BROWN;
						break;
					case 'B':	/* Blue */
						curattr=((curattr)&0xf8)|this.defs.BLUE;
						break;
					case 'M':	/* Magenta */
						curattr=((curattr)&0xf8)|this.defs.MAGENTA;
						break;
					case 'C':	/* Cyan */
						curattr=((curattr)&0xf8)|this.defs.CYAN;
						break;
					case 'W':	/* White */
						curattr=((curattr)&0xf8)|this.defs.LIGHTGRAY;
						break;
					case '0':	/* Black */
						curattr=(curattr)&0x8f;
						break;
					case '1':	/* Red */
						curattr=((curattr)&0x8f)|(this.defs.RED<<4);
						break;
					case '2':	/* Green */
						curattr=((curattr)&0x8f)|(this.defs.GREEN<<4);
						break;
					case '3':	/* Yellow */
						curattr=((curattr)&0x8f)|(this.defs.BROWN<<4);
						break;
					case '4':	/* Blue */
						curattr=((curattr)&0x8f)|(this.defs.BLUE<<4);
						break;
					case '5':	/* Magenta */
						curattr=((curattr)&0x8f)|(this.defs.MAGENTA<<4);
						break;
					case '6':	/* Cyan */
						curattr=((curattr)&0x8f)|(this.defs.CYAN<<4);
						break;
					case '7':	/* White */
						curattr=((curattr)&0x8f)|(this.defs.LIGHTGRAY<<4);
						break;
					case 'H':	/* High Intensity */
						curattr|=this.defs.HIGH;
						break;
					case 'I':	/* Blink */
						curattr|=this.defs.BLINK;
						break;
					case 'N':	/* Normal (ToDo: Does this do ESC[0?) */
						curattr=7;
						break;
					case '-':	/* Normal if High, Blink, or BG */
						if(curattr & 0xf8)
							curattr=7;
						break;
					case '_':	/* Normal if blink/background */
						if(curattr & 0xf0)
							curattr=7;
						break;
					case '[':	/* CR */
						x=0;
						break;
					case ']':	/* LF */
						y++;
						break;
					default:	/* Other stuff... specifically, check for right movement */
						if(ch.charCodeAt(0)>127) {
							x+=ch.charCodeAt(0)-127;
							if(x>=this.width)
								x=this.width-1;
						}
				}
				break;
			case '\x07':		/* Beep */
				break;
			case '\r':
				x=0;
				break;
			case '\n':
				y++;
				break;
			default:
				this.data[x][y]=new this.Cell(ch,curattr);
				x++;
				if(x>=this.width) {
					x=0;
					y++;
					if(txt[p] == '\r') p++;
					if(txt[p] == '\n') p++;
				}
		}
	}
	return(scrolls);
};

/*
 * Returns an array of base 64 encoded strings.
 * Argument: the maximum length of the encoded lines (default: 76 chars)
 *
 * TODO: Delete
 */
Graphic.prototype.base64_encode = function(max_line_len)
{
	if(!max_line_len)
		max_line_len = 76;
	return base64_encode(this.BIN).match(new RegExp('([\x00-\xff]{1,' + max_line_len + '})', 'g'));
};

/*
 * Like parseANSI() for b64 data
 *
 * TODO: Delete
 */
Graphic.prototype.base64_decode = function(rows)
{
	this.BIN = base64_decode(rows.join(""));
};

// Fixes anomalies in "ANSI art" invisible to artists (but visible when viewed without colors)
// where the artist used an inverse-space instead of a solid block, for example.
// The normalized graphic should appear the same as the original when viewed in color
// and the same or better without color.
Graphic.prototype.normalize = function(bg)
{
	if(bg == undefined)
		bg = this.data[0][0].attr;
	bg &= 0xf0;
	for (var y=0; y<this.height; y++) {
		for (var x=0; x<this.width; x++) {
			// Colored-BG space? Replace with full block
			if(bg == 0 && this.data[x][y].ch == ' ' && (this.data[x][y].attr&0xf0) != 0) {
				this.data[x][y].ch = ascii(219);
				this.data[x][y].attr >>= 4;	// Original foreground color discarded (irrelevant)
				continue;
			}
			if((this.data[x][y].attr&0x0f) << 4 != bg)
				continue;
			if((this.data[x][y].attr&0xf0) == bg)
				continue;
			var ch = this.data[x][y].ch;
			switch(ascii(this.data[x][y].ch)) {
				case 219:
					ch = ascii(32);
					break;
				case 32:
					ch = ascii(219);
					break;
				case 220:
					ch = ascii(223);
					break;
				case 223:
					ch = ascii(220);
					break;
				case 221:
					ch = ascii(222);
					break;
				case 222:
					ch = ascii(221);
					break;
			}
			if(ch != this.data[x][y].ch) {
				this.data[x][y].ch = ch;
				this.data[x][y].attr >>= 4;
				this.data[x][y].attr |= bg;
			}
		}
	}
	return this;
}

// Does not modify HIGH-intensity or BLINK attributes:
Graphic.prototype.change_colors = function(fg, bg)
{
	for(var y=0; y<this.height; y++) {
		for(var x=0; x<this.width; x++) {
			attr = this.data[x][y].attr;
			if(fg !== undefined) {
				attr &= 0xf8;
				attr |= (fg&7);
			}
			if(bg !== undefined) {
				attr &= 0x8f;
				attr |= (bg&7) << 4;
			}
			this.data[x][y].attr = attr;
		}
	}
}

/* Leave as last line for convenient load() usage: */
Graphic;
