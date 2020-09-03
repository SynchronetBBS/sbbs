// $Id: graphic.js,v 1.13 2019/09/03 05:26:50 deuce Exp $

/*
 * "Graphic" object
 * Allows a graphic to be stored in memory and portions of it redrawn on command
 */

/*
 * Class that represents a drawable object.
 * w = width (default of 80)
 * h = height (default of 24)
 * attr = default attribute (default of 7 ie: LIGHTGREY)
 * ch = default character (default of space)
 *
 * Instance variable data contains an array of array of Graphics.Cell objects
 *
 */

/*jslint for*/

require('attribute.js', 'Attribute');

function Graphic(w,h,attr,ch, puttext)
{
	'use strict';
	var x;
	var y;

	if (puttext === undefined) {
		puttext = false;
	}
	if (puttext) {
		this.keep_puttext = true;
	}
	if(ch===undefined) {
		this.ch=' ';
	}
	else {
		this.ch=ch;
	}
	if(attr===undefined) {
		attr=7;
	}
	this.attribute = new Attribute(attr);
	if(h===undefined) {
		this.height=24;
	}
	else {
		this.height=h;
	}
	if(w===undefined) {
		this.width=80;
	}
	else {
		this.width=w;
	}

	this.data=new Array(this.width);
	if (this.keep_puttext) {
		this.puttext = new Array(this.width * this.height * 2);
	}

	for(y=0; y<this.height; y += 1) {
		for(x=0; x<this.width; x += 1) {
			if(y===0) {
				this.data[x]=new Array(this.height);
			}
			this.data[x][y]=new this.Cell(this.ch,this.attribute);
			if (this.keep_puttext) {
				this.puttext[(y*this.width+x)*2] = ascii(this.ch);
				this.puttext[(y*this.width+x)*2+1] = this.attribute.value;
			}
		}
	}
}

/*
 * The Cell subclass which contains the attribute and character for a
 * single cell.
 */
Graphic.prototype.Cell = function(ch,attr)
{
	'use strict';
	this.ch=ch;
	this.attr=new Attribute(attr);
};

Graphic.prototype.setCell = function(ch, attr, x, y)
{
	'use strict';
	if (this.data[x][y] === undefined) {
		throw(x+'/'+y+' is undefined!  Size: '+this.width+'/'+this.height);
	}

	this.data[x][y].ch = ch;
	this.data[x][y].attr.value = attr;
	if (this.keep_puttext) {
		this.puttext[(y*this.width+x)*2+1] = attr;
		this.puttext[(y*this.width+x)*2] = ascii(ch);
	}
};

/*
 * BIN property is the string representation of the graphic in a series of
 * char/attr pairs.
 */
Object.defineProperty(Graphic.prototype, "BIN", {
	get: function() {
		'use strict';
		var bin = '';
		var x;
		var y;

		for (y=0; y<this.height; y += 1) {
			for (x=0; x<this.width; x += 1) {
				bin += this.data[x][y].ch;
				bin += ascii(this.data[x][y].attr.value);
			}
		}
		return bin;
	},
	set: function(bin) {
		'use strict';
		var x;
		var y;
		var pos = 0;
		var blen = bin.length;

		for (y=0; y<this.height; y += 1) {
			for (x=0; x<this.width; x += 1) {
				if (blen >= pos+2) {
					this.setCell(bin.charAt(pos), bin.charCodeAt(pos+1));
				}
				else {
					return;
				}
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
		'use strict';
		var x;
		var y;
		var curattr=7;
		var ansi = '';
		var char;

		for(y=0; y<this.height; y += 1) {
			for(x=0; x<this.width; x += 1) {
				ansi += this.data[x][y].attr.ansi(curattr);
            	curattr = this.data[x][y].attr;
            	char = this.data[x][y].ch;
            	/* Don't put printable chars in the last column */
//            	if(char === ' ' || (x<this.width-1))
                	ansi += char;
        	}
			ansi += '\r\n';
    	}
    	return ansi;
	},
	set: function(ans) {
		'use strict';
		var attr = new Attribute(this.attribute);
		var saved = {};
		var x = 0;
		var y = 0;
		var std_cmds = {
			'm':function(params) {
				if (params[0] === undefined || params[0] === '') {
					params[0] = 0;
				}

				while (params.length) {
					switch (parseInt(params[0], 10)) {
						case 0:
							attr.value = 7;
							break;
						case 1:
							attr.bright = true;
							break;
						case 40:
							attr.bg = Attribute.BLACK;
							break;
						case 41:
							attr.bg = Attribute.RED;
							break;
						case 42: 
							attr.bg = Attribute.GREEN;
							break;
						case 43:
							attr.bg = Attribute.BROWN;
							break;
						case 44:
							attr.bg = Attribute.BLUE;
							break;
						case 45:
							attr.bg = Attribute.MAGENTA;
							break;
						case 46:
							attr.bg = Attribute.CYAN;
							break;
						case 47:
							attr.bg = Attribute.WHITE;
							break;
						case 30:
							attr.fg = Attribute.BLACK;
							break;
						case 31:
							attr.fg = Attribute.RED;
							break;
						case 32:
							attr.fg = Attribute.GREEN;
							break;
						case 33:
							attr.fg = Attribute.BROWN;
							break;
						case 34:
							attr.fg = Attribute.BLUE;
							break;
						case 35:
							attr.fg = Attribute.MAGENTA;
							break;
						case 36:
							attr.fg = Attribute.CYAN;
							break;
						case 37:
							attr.fg = Attribute.LIGHTGRAY;
							break;
					}
					params.shift();
				}
			},
			'H':function(params, obj) {
				if (params[0] === undefined || params[0] === '') {
					params[0] = 1;
				}
				if (params[1] === undefined || params[1] === '') {
					params[1] = 1;
				}

				y = parseInt(params[0], 10) - 1;
				if (y < 0) {
					y = 0;
				}
				if (y >= obj.height) {
					y = obj.height-1;
				}

				x = parseInt(params[1], 10) - 1;
				if (x < 0) {
					x = 0;
				}
				if (x >= obj.width) {
					x = obj.width-1;
				}
			},
			'A':function(params) {
				if (params[0] === undefined || params[0] === '') {
					params[0] = 1;
				}
	
				y -= parseInt(params[0], 10);
				if (y < 0) {
					y = 0;
				}
			},
			'B':function(params, obj) {
				if (params[0] === undefined || params[0] === '') {
					params[0] = 1;
				}

				y += parseInt(params[0], 10);
				if (y >= obj.height) {
					y = obj.height-1;
				}
			},
			'C':function(params, obj) {
				if (params[0] === undefined || params[0] === '') {
					params[0] = 1;
				}

				x += parseInt(params[0], 10);
				if (x >= obj.width) {
					x = obj.width - 1;
				}
			},
			'D':function(params) {
				if (params[0] === undefined || params[0] === '') {
					params[0] = 1;
				}

				x -= parseInt(params[0], 10);
				if (x < 0) {
					x = 0;
				}
			},
			'J':function(params,obj) {
				if (params[0] === undefined || params[0] === '') {
					params[0] = 0;
				}

				if (parseInt(params[0], 10) === 2) {
					obj.clear();
				}
			},
			's':function() {
				saved={'x':x, 'y':y};
			},
			'u':function() {
				x = saved.x;
				y = saved.y;
			}
		};
		std_cmds.f = std_cmds.H;
		var m;
		var paramstr;
		var cmd;
		var params;
		var ch;

		/* parse 'ATCODES'?? 
		ans = ans.replace(/@(.*)@/g,
			function (str, code, offset, s) {
				return bbs.atcode(code);
			}
		);
		*/
		while(ans.length > 0) {
			m = ans.match(/^\x1b\[([\x30-\x3f]*)([\x20-\x2f]*[\x40-\x7e])/);
			if (m !== null) {
				paramstr = m[1];
				cmd = m[2];
				if (paramstr.search(/^[<=>?]/) !== 0) {
					params=paramstr.split(/;/);

					if (std_cmds[cmd] !== undefined) {
						std_cmds[cmd](params,this);
					}
				}
				ans = ans.substr(m[0].length);
			}
			else {

				/* set character and attribute */
				ch = ans[0];
				ans = ans.substr(1);

				/* Handle non-ANSI cursor positioning control characters */
				switch(ch) {
					case '\r':
						x=0;
						break;
					case '\n':
						y += 1;
						break;
					case '\t':
						x += 8-(x%8);
						break;
					case '\b':
						if(x > 0) {
							x -= 1;
						}
						break;
					default:
						this.setCell(ch, attr, x, y);
						x += 1;
				}
				/* validate position/scroll */
				if (x < 0) {
					x = 0;
				}
				if (y < 0) {
					y = 0;
				}
				if(x>=this.width) {
					x=0;
					y += 1;
				}
				while(y >= this.height) {
					this.scroll();
					y -= 1;
				}
			}
		}
	}
});

/*
 * Returns an HTML encoding of the graphic data.
 *
 * TODO: get rid of the ANSI phase as unnecessary
 */
Object.defineProperty(Graphic.prototype, "HTML", {
	get: function() {
		'use strict';
		return html_encode(this.ANSI, /* ex-ascii: */true, /* whitespace: */false, /* ansi: */true, /* Ctrl-A: */false);
	}
});

/*
 * Gets a portion of the Graphic object as a new Graphic object
 */
Graphic.prototype.get = function(sx, sy, ex, ey)
{
	'use strict';
	var ret;
	var x;
	var y;

	if (sx < 0 || sy < 0 || sx >= this.width || sy > this.height
			|| ex < 0 || ey < 0 || ex >= this.width || ey > this.height
			|| ex < sx || ey < sy) {
		return undefined;
	}

	ret = new Graphic(ex-sx+1, ey-sy+1, this.attr, this.ch);
	for (x=sx; x<=ex; x += 1) {
		for (y=sy; y<=ey; y += 1) {
			ret.data[x-sx][y-sy].ch = this.data[x][y].ch;
			ret.data[x-sx][y-sy].attr.value = this.data[x][y].attr.value;
		}
	}
	return ret;
};

/*
 * Moves a portion of the Graphic object to alother location in the same
 * Graphic object.
 */
Graphic.prototype.copy = function(sx, sy, ex, ey, tx, ty)
{
	'use strict';
	var x;
	var y;
	var xdir;
	var ydir;
	var xstart;
	var ystart;
	var xend;
	var yend;
	var txstart;
	var tystart;

	if (sx < 0 || sy < 0 || sx >= this.width || sy > this.height
			|| ex < 0 || ey < 0 || ex >= this.width || ey > this.height
			|| ex < sx || ey < sy || tx < 0 || ty < 0 || tx + (ex - sx) >= this.width
			|| ty + (ey - sy) >= this.height) {
		return false;
	}

	if (sx <= tx) {
		xdir = -1;
		xstart = ex;
		xend = sx - 1;
		txstart = tx+(ex - sx);
	}
	else {
		xdir = 1;
		xstart = sx;
		xend = ex + 1;
		txstart = tx;
	}
	if (sy <= ty) {
		ydir = -1;
		ystart = ey;
		yend = sy - 1;
		tystart = ty+(ey - sy);
	}
	else {
		ydir = 1;
		ystart = sy;
		yend = ey + 1;
		tystart = ty;
	}

	for (y=ystart; y !== yend; y += ydir) {
		for (x=xstart; x !== xend; x += xdir) {
			this.setCell(this.data[x][y].ch, this.data[x][y].attr.value, txstart + (x-xstart), tystart + (y-ystart));
		}
	}
	return true;
};

/*
 * Puts a graphic object into this one.
 */
Graphic.prototype.put = function(gr, x, y)
{
	'use strict';
	var gx;
	var gy;

	if (x < 0 || y < 0 || x+gr.width > this.width || y+gr.height > this.height) {
		return false;
	}

	for (gx = 0; gx < gr.width; gx += 1) {
		for (gy = 0; gy < gr.height; gy += 1) {
			this.setCell(gr.data[gx][gy].ch, gr.data[gx][gy].attr.value, x, y);
		}
	}
};

/*
 * Resets the graphic to all this.ch/this.attr Cells
 */
Graphic.prototype.clear = function()
{
	'use strict';
	var x;
	var y;

	for(y=0; y<this.height; y += 1) {
		for(x=0; x<this.width; x += 1) {
			this.setCell(this.ch, this.attribute.value, x, y);
		}
	}
};

/*
 * Draws the graphic using the cons object
 */
Graphic.prototype.draw = function(xpos,ypos,width,height,xoff,yoff,cons)
{
	'use strict';
	var x;
	var y;
	var ch;

	if(xpos===undefined) {
		xpos=0;
	}
	if(ypos===undefined) {
		ypos=0;
	}
	if(width===undefined) {
		width=this.width;
	}
	if(height===undefined) {
		height=this.height;
	}
	if(xoff===undefined) {
		xoff=0;
	}
	if(yoff===undefined) {
		yoff=0;
	}
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false);
	}
	if(xpos+width > cons.cols || ypos+height > cons.rows) {
		alert("Attempt to draw outside of screen: " + (xpos+width-1) + "x" + (ypos+height-1));
		return(false);
	}
	for(y=0;y<height; y += 1) {
		cons.gotoxy(xpos,ypos+y);
		for(x=0; x<width; x += 1) {
			// Do not draw to the bottom left corner of the screen-would scroll
			if(xpos+x !== cons.cols
					|| ypos+y !== cons.rows) {
				cons.attr = this.data[x+xoff][y+yoff].attr;
				ch=this.data[x+xoff][y+yoff].ch;
				if(ch === "\r" || ch === "\n" || !ch) {
					ch=this.ch;
				}
				cons.print(ch);
			}
		}
	}
	return(true);
};

/*
 * Loads a ANS or BIN file.
 * TODO: The ASC load is pretty much guaranteed to be broken.
 */
Graphic.prototype.load = function(filename)
{
	'use strict';
	var file_type=file_getext(filename).substr(1);
	var f=new File(filename);
	var lines;

	switch(file_type.toUpperCase()) {
	case "ANS":
		if(!(f.open("rb",true))) {
			return(false);
		}
		this.ANSI = f.read();
		f.close();
		break;
	case "BIN":
		if(!(f.open("rb",true))) {
			return(false);
		}
		this.BIN = f.read();
		f.close();
		break;
	case "ASC":
		if(!(f.open("r",true,4096))) {
			return(false);
		}
		lines=f.readAll();
		f.close();
		lines.forEach(function(l) {
			this.putmsg(undefined,undefined,l,true);
		});
		break;
	default:
		throw("unsupported file type:" + filename);
	}
	return(true);
};

/*
 * The inverse of load()... saves a BIN representation to a file.
 */
Graphic.prototype.save = function(filename)
{
	'use strict';
	var f=new File(filename);

	if(!(f.open("wb",true))) {
		return(false);
	}
	f.write(this.BIN);
	f.close();
	return(true);
};

Graphic.prototype.scrollup = function()
{
	'use strict';
	var x;

	for (x = 0; x < this.width; x += 1) {
		this.data[x].shift();
		this.data[x].push(new this.Cell(this.ch,this.attribute));
	}
	if (this.keep_puttext) {
		this.puttext.splice(0, this.width * 2);
		while (this.puttext.length < this.width * this.height * 2) {
			this.puttext.push(ascii(this.ch));
			this.puttext.push(this.attribute.value);
		}
	}
};
