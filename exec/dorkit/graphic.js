// $Id$

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

if (js.global.Attribute === undefined)
	load("attribute.js");

function Graphic(w,h,attr,ch)
{
	if(ch==undefined)
		this.ch=' ';
	else
		this.ch=ch;
	if(attr==undefined)
		attr=7;
	this.attribute = new Attribute(attr);
	if(h==undefined)
		this.height=24;
	else
		this.height=h;
	if(w==undefined)
		this.width=80;
	else
		this.width=w;

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

/*
 * The Cell subclass which contains the attribute and character for a
 * single cell.
 */
Graphic.prototype.Cell = function(ch,attr)
{
	this.ch=ch;
	this.attr=new Attribute(attr);
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
				if (blen >= pos+2)
					this.data[x][y] = new this.Cell(bin.charAt(pos), bin.charCodeAt(pos+1));
				else
					return;
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
				ansi += this.data[x][y].attr.ansi(curattr);
            	curattr = this.data[x][y].attr;
            	char = this.data[x][y].ch;
            	/* Don't put printable chars in the last column */
//            	if(char == ' ' || (x<this.width-1))
                	ansi += char;
        	}
			ansi += '\r\n';
    	}
    	return ansi;
	},
	set: function(ans) {
		var attr = new Attribute(this.attribute);
		var saved = {};
		var x = 0;
		var y = 0;
		var std_cmds = {
			'm':function(params, obj) {
				if (params[0] === undefined || params[0] === '')
					params[0] = 0;

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
				x = saved.x;
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
				if (paramstr.search(/^[<=>?]/) != 0) {
					params=paramstr.split(/;/);

					if (std_cmds[cmd] !== undefined)
						std_cmds[cmd](params,this);
				}
				ans = ans.substr(m[0].length);
   	            continue;
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
                default:
			        this.data[x][y]=new this.Cell(ch,attr);
			        x++;
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
				this.scroll();
				y--;
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
        return html_encode(this.ANSI, /* ex-ascii: */true, /* whitespace: */false, /* ansi: */true, /* Ctrl-A: */false);
    }
});

/*
 * Gets a portion of the Graphic object as a new Graphic object
 */
Graphic.prototype.get = function(sx, sy, ex, ey)
{
	var ret;
	var x;
	var y;

	if (sx < 0 || sy < 0 || sx >= this.width || sy > this.height
			|| ex < 0 || ey < 0 || ex >= this.width || ey > this.height
			|| ex < sx || ey < sy)
		return undefined;

	ret = new Graphic(ex-sx+1, ey-sy+1, this.attr, this.ch);
	for (x=sx; x<=ex; x++) {
		for (y=sy; y<=ey; y++) {
			ret.data[x-sx][y-sy].ch = this.data[x][y].ch;
			ret.data[x-sx][y-sy].attr = new Attribute(this.data[x][y].attr);
		}
	}
	return ret;
}

/*
 * Puts a graphic object into this one.
 */
Graphic.prototype.put = function(gr, x, y)
{
	var gx;
	var gy;

	if (x < 0 || y < 0 || x+gr.width > this.width || y+gr.height > this.height)
		return false;

	for (gx = 0; gx < gr.width; gx++) {
		for (gy = 0; gy < gr.height; gy++) {
			this.data[x+gx][y+gy].ch = gr.data[gx][gy].ch;
			this.data[x+gx][y+gy].attr = new Attribute(gr.data[gx][gy].attr);
		}
	}
}

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
 * Draws the graphic using the console object
 */
Graphic.prototype.draw = function(xpos,ypos,width,height,xoff,yoff)
{
	var x;
	var y;
	var ch;

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
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false);
	}
	if(xpos+width-1 > dk.console.cols || ypos+height-1 > dk.console.rows) {
		alert("Attempt to draw outside of screen: " + (xpos+width-1) + "x" + (ypos+height-1));
		return(false);
	}
	for(y=0;y<height; y++) {
		dk.console.gotoxy(xpos,ypos+y);
		for(x=0; x<width; x++) {
			// Do not draw to the bottom left corner of the screen-would scroll
			if(xpos+x != dk.console.cols
					|| ypos+y != dk.console.rows) {
				dk.console.attr = this.data[x+xoff][y+yoff].attr;
				ch=this.data[x+xoff][y+yoff].ch;
				if(ch == "\r" || ch == "\n" || !ch)
					ch=this.ch;
				dk.console.print(ch);
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
	var file_type=file_getext(filename).substr(1);
	var f=new File(filename);
	var l;

	switch(file_type.toUpperCase()) {
	case "ANS":
		if(!(f.open("rb",true)))
			return(false);
		this.ANSI = f.read();
		f.close();
		break;
	case "BIN":
		if(!(f.open("rb",true)))
			return(false);
		this.BIN = f.read();
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
		throw("unsupported file type:" + filename);
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

/* Leave as last line for convenient load() usage: */
Graphic;
