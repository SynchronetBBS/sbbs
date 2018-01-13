/* $Id$ */

/**
 	Javascript Frame Library 					
 	for Synchronet v3.15a+ 
 	by Matt Johnson (mcmlxxix)	

DESCRIPTION:

 	this library is meant to be used in conjunction with other libraries that
 	store display data in a Frame() object or objects
 	this allows for "windows" that can be hidden, moved, closed, etc...
	without destroying the data behind them.

 	the object itself takes the following parameters:

 		x: 			the coordinate representing the top left corner of the frame (horiz)
 		y: 			the coordinate representing the top left corner of the frame (vert)
 		width: 		the horizontal width of the frame 
 		height: 	the vertical height of the frame
 		attr:		the default color attributes of the frame
		parent:		a frame object representing the parent of the new frame
		
METHODS:

	frame.open()				//populate frame contents in character canvas
	frame.close()				//remove frame contents from character canvas
	frame.delete()				//delete this frame (remove from screen buffer, destroy internal references)
	frame.invalidate()			//clear screen buffer to redraw contents on cycle() or draw()
	frame.draw()				//open frame (if not open) move to top (if not on top) and cycle()
	frame.cycle()				//check the display matrix for updated characters and displays them 
	frame.refresh()				//flag all frame sectors for potential update
	frame.load(filename)		//load a binary graphic (.BIN) or ANSI graphic (.ANS) file
	frame.bottom()				//push frame to bottom of display stack
	frame.top()					//pull frame to top of display stack
	frame.scroll(x,y)			//scroll frame n spaces in any direction
	frame.scrollTo(x,y)			//scroll frame to absolute offset
	frame.move(x,y)				//move frame n spaces in any direction
	frame.moveTo(x,y)			//move frame to absolute position
	frame.end()					//opposite of frame.home()
	frame.screenShot(file,append)
								//capture the contents of a frame to file
	frame.getData(x,y,use_offset)
								//return the character and attribute located at x,y in frame.data (optional scroll offset)
	frame.setData(x,y,ch,attr,use_offset)
								//modify the character and attribute located at x,y in frame.data (optional scroll offset)
	frame.clearData(x,y,use_offset)
								//delete the character and attribute located at x,y in frame.data (optional scroll offset)
	frame.clearline(attr)		//see http://synchro.net/docs/jsobjs.html#console
	frame.cleartoeol(attr)
	frame.putmsg(str,attr)
	frame.clear(attr)
	frame.home()
	frame.center(str,attr)
	frame.crlf()
	frame.getxy()
	frame.gotoxy(x,y)
	frame.pushxy()
	frame.popxy()
	
PROPERTIES:

	frame.x						//x screen position
	frame.y						//y screen position
	frame.width					//frame width
	frame.height				//frame height
	frame.data					//frame data matrix 
	frame.data_height			//true height of frame contents (READ ONLY)
	frame.data_width			//true width of frame contents (READ ONLY)
	frame.attr					//default attributes for frame
	frame.checkbounds			//toggle true/false to restrict/allow frame movement outside display
	frame.lf_strict				//toggle true/false to force newline after a crlf-terminated string
	frame.v_scroll				//toggle true/false to enable/disable vertical scrolling
	frame.h_scroll				//toggle true/false to enable/disable horizontal scrolling
	frame.scrollbars			//toggle true/false to show/hide scrollbars
	frame.transparent			//toggle true/false to enable transparency mode 
								//(do not display frame sectors where char == undefined)
	frame.offset				//current offset object {x,y}
	frame.cursor				//current cursor object {x,y}
	frame.parent				//the parent frame of a frame
	frame.id					//a unique identifier (e.g. "0.1.1.2.3")
	frame.is_open				//return true is frame is opened in screen buffer

USAGE:

	//create a new frame object at screen position 1,1. 80 characters wide by 24 tall
 	load("frame.js");
 	var frame = new Frame(1,1,80,24,BG_BLUE);
	
	//add frame to the display canvas
	frame.open();
 
	//add a new frame within the frame object that will display on top at position 10,10
	var subframe = new Frame(10,10,10,10,BG_GREEN,frame);
	
	//add subframe to the display canvas
	subframe.open();
	
	//place cursor at position x:5 y:5 relative to subframe's coordinates
	subframe.gotoxy(5,5);

	//beware this sample infinite loop
 	while(!js.terminated) { 
		//print a message into subframe
		subframe.putmsg("1");
		
		//on first call this will draw the entire initial frame, 
		//as triggered by the open() method call.
		//on subsequent calls this will draw only areas that have changed
		frame.cycle();
		//NOTE: if frames are linked, only one frame needs to be cycled
		//		for all frames to update
	}
	
	//close out the entire frame tree
	frame.close();
	
 */
 
load("sbbsdefs.js");
function Frame(x,y,width,height,attr,parent) {

	/* private properties */
	this.__properties__ = {
		x:undefined,
		y:undefined,
		width:undefined,
		height:undefined,
		attr:undefined,
		display:undefined,
		data:[],
		open:false,
		ctrl_a:false,
		curr_attr:undefined,
		id:0
	};
	this.__settings__ = {
		v_scroll:true,
		h_scroll:false,
		scrollbars:false,
		lf_strict:true,
		checkbounds:true,
		transparent:false,
		word_wrap:false
	};
	this.__relations__ = {
		parent:undefined,
		child:[]
	};
	this.__position__ = {
		cursor:undefined,
		offset:undefined,
		stored:undefined
	};
		
	function init(x,y,width,height,attr,parent) {
		if(parent instanceof Frame) {
			this.__properties__.id = parent.display.nextID;
			this.__properties__.display = parent.display;
			this.__settings__.checkbounds = parent.checkbounds;
			this.__relations__.parent = parent;
			parent.child = this;
		}
		else {
			this.__properties__.display = new Display(x,y,width,height);
		}
		
		this.x = x;
		this.y = y;
		this.width = width;
		this.height = height;
		this.attr = attr;
		
		this.__position__.cursor = new Cursor(0,0,this);
		this.__position__.offset = new Offset(0,0,this);
		this.__position__.stored = new Cursor(0,0,this);

		//log(LOG_DEBUG,format("new frame initialized: %sx%s at %s,%s",this.width,this.height,this.x,this.y));
	}
	init.apply(this,arguments);
}

/* protected properties */
Frame.prototype.__defineGetter__("child", function() {
	return this.__relations__.child;
});
Frame.prototype.__defineSetter__("child", function(frame) {
	if(frame instanceof Frame)
		this.__relations__.child.push(frame);
	else
		throw("child not an instance of Frame()");
});
Frame.prototype.__defineGetter__("attr", function() {
	return this.__properties__.attr;
});
Frame.prototype.__defineSetter__("attr", function(attr) {
	if(attr !== undefined && isNaN(attr))
		throw("invalid attribute: " + attr);
	this.__properties__.attr = attr;
});
Frame.prototype.__defineGetter__("x", function() { 
	if(this.__properties__.x == undefined)
		return this.__properties__.display.x; 
	return this.__properties__.x;
});
Frame.prototype.__defineSetter__("x", function(x) {
	if(x == undefined)
		return;
	if(!this.__checkX__(x))
		throw("invalid x coordinate: " + x);
	this.__properties__.x = Number(x);
});
Frame.prototype.__defineGetter__("y", function() { 
	if(this.__properties__.y == undefined)
		return this.__properties__.display.y; 
	return this.__properties__.y;
});
Frame.prototype.__defineSetter__("y", function(y) {
	if(y == undefined)
		return;
	if(!this.__checkY__(y))
		throw("invalid y coordinate: " + y);
	this.__properties__.y = Number(y);
});
Frame.prototype.__defineGetter__("width", function() {
	if(this.__properties__.width == undefined)
		return this.__properties__.display.width;
	return this.__properties__.width;
});
Frame.prototype.__defineSetter__("width", function(width) {
	if(width == undefined)
		return;
	if(!this.__checkWidth__(this.x,Number(width)))
		throw("invalid width: " + width);
	this.__properties__.width = Number(width);
});
Frame.prototype.__defineGetter__("height", function() {
	if(this.__properties__.height == undefined)
		return this.__properties__.display.height;
	return this.__properties__.height;
});
Frame.prototype.__defineSetter__("height", function(height) {
	if(height == undefined)
		return;
	if(!this.__checkHeight__(this.y,Number(height)))
		throw("invalid height: " + height);
	this.__properties__.height = Number(height);
});

/* read-only properties */
Frame.prototype.__defineGetter__("cursor",function() {
	return this.__position__.cursor;
});
Frame.prototype.__defineGetter__("offset",function() {
	return this.__position__.offset;
});
Frame.prototype.__defineGetter__("id", function() {
	return this.__properties__.id;
});
Frame.prototype.__defineGetter__("parent", function() {
	return this.__relations__.parent;
});
Frame.prototype.__defineGetter__("display", function() {
	return this.__properties__.display;
});
Frame.prototype.__defineGetter__("data_height", function() {
	return this.__properties__.data.length;
});
Frame.prototype.__defineGetter__("data_width", function() {
	if(typeof this.__properties__.data[0] == "undefined")
		return 0;
	var longest = 0;
	for(var d in this.__properties__.data) {
		if(this.__properties__.data[d].length > longest)
			longest = this.__properties__.data[d].length;
	}
	return longest + 1;
});
Frame.prototype.__defineGetter__("data", function() {
	return this.__properties__.data;
});

/* protected settings */
Frame.prototype.__defineGetter__("checkbounds", function() {
	return this.__settings__.checkbounds;
});
Frame.prototype.__defineSetter__("checkbounds", function(bool) {
	if(typeof bool == "boolean")
		this.__settings__.checkbounds=bool;
	else
		throw("non-boolean checkbounds: " + bool);
});
Frame.prototype.__defineGetter__("transparent", function() {
	return this.__settings__.transparent;
});
Frame.prototype.__defineSetter__("transparent", function(bool) {
	if(typeof bool == "boolean")
		this.__settings__.transparent=bool;
	else
		throw("non-boolean transparent: " + bool);
});	
Frame.prototype.__defineGetter__("lf_strict", function() {
	return this.__settings__.lf_strict;
});
Frame.prototype.__defineSetter__("lf_strict", function(bool) {
	if(typeof bool == "boolean")
		this.__settings__.lf_strict=bool;
	else
		throw("non-boolean lf_strict: " + bool);
});
Frame.prototype.__defineGetter__("scrollbars", function() {
	return this.__settings__.scrollbars;
});
Frame.prototype.__defineSetter__("scrollbars", function(bool) {
	if(typeof bool == "boolean")
		this.__settings__.scrollbars=bool;
	else
		throw("non-boolean scrollbars: " + bool);
});
Frame.prototype.__defineGetter__("v_scroll", function() {
	return this.__settings__.v_scroll;
});
Frame.prototype.__defineSetter__("v_scroll", function(bool) {
	if(typeof bool == "boolean")
		this.__settings__.v_scroll=bool;
	else
		throw("non-boolean v_scroll: " + bool);
});
Frame.prototype.__defineGetter__("word_wrap", function() {
	return this.__settings__.word_wrap;
});
Frame.prototype.__defineSetter__("word_wrap", function(bool) {
	if(typeof bool == "boolean")
		this.__settings__.word_wrap=bool;
	else
		throw("non-boolean word_wrap: " + bool);
});
Frame.prototype.__defineGetter__("h_scroll", function() {
	return this.__settings__.h_scroll;
});
Frame.prototype.__defineSetter__("h_scroll", function(bool) {
	if(typeof bool == "boolean")
		this.__settings__.h_scroll=bool;
	else
		throw("non-boolean h_scroll: " + bool);
});
Frame.prototype.__defineGetter__("is_open",function() {
	return this.__properties__.open;
});

/* public methods */
Frame.prototype.getData = function(x,y,use_offset) {
	var px = x;
	var py = y;
	if(use_offset) {
		px += this.__position__.offset.x;
		py += this.__position__.offset.y;
	}
	// if(!this.__properties__.data[py] || !this.__properties__.data[py][px])
		// throw("Frame.getData() - invalid coordinates: " + px + "," + py);
	if(!this.__properties__.data[py] || !this.__properties__.data[py][px])
		return new Char();
	return this.__properties__.data[py][px];
}
Frame.prototype.setData = function(x,y,ch,attr,use_offset) {
	var px = x;
	var py = y;
	if(use_offset) {
		px += this.__position__.offset.x;
		py += this.__position__.offset.y;
	}
	//I don't remember why I did this, but it was probably important at the time
	//if(!this.__properties__.data[py] || !this.__properties__.data[py][px])
		// throw("Frame.setData() - invalid coordinates: " + px + "," + py);
	if(!this.__properties__.data[py]) 
		this.__properties__.data[py] = [];
	if(!this.__properties__.data[py][px]) 
		this.__properties__.data[py][px] = new Char();
	if(this.__properties__.data[py][px].ch == ch && this.__properties__.data[py][px].attr == attr)
		return;
	if(ch)
		this.__properties__.data[py][px].ch = ch;
	if(attr)
		this.__properties__.data[py][px].attr = attr;
	if(this.__properties__.open) 
		this.__properties__.display.updateChar(this,x,y);
}
Frame.prototype.clearData = function(x,y,use_offset) {
	var px = x;
	var py = y;
	if(use_offset) {
		px += this.__position__.offset.x;
		py += this.__position__.offset.y;
	}
	if(!this.__properties__.data[py] || !this.__properties__.data[py][px])
		return;
	else if(this.__properties__.data[py][px].ch == undefined && this.__properties__.data[py][px].attr == undefined)
		return;
	this.__properties__.data[py][px].ch = undefined;
	this.__properties__.data[py][px].attr = undefined;
	if(this.__properties__.open) 
		this.__properties__.display.updateChar(this,x,y);
}
Frame.prototype.bottom = function() {
	if(this.__properties__.open) {
		for each(var c in this.__relations__.child) 
			c.bottom();
		this.__properties__.display.bottom(this);
	}
}
Frame.prototype.top = function() {
	if(this.__properties__.open) {
		this.__properties__.display.top(this);
		for each(var c in this.__relations__.child) 
			c.top();
	}
}
Frame.prototype.open = function() {
	if(!this.__properties__.open) {
		this.__properties__.display.open(this);
		this.__properties__.open = true;
	}
	for each(var c in this.__relations__.child) {
		c.open();
	}
}
Frame.prototype.refresh = function() {
	if(this.__properties__.open) {
		this.__properties__.display.updateFrame(this);
		for each(var c in this.__relations__.child) 
			c.refresh();
	}
}
Frame.prototype.close = function() {
	for each(var c in this.__relations__.child) 
		c.close();
	if(this.__properties__.open) {
		this.__properties__.display.close(this);
		this.__properties__.open = false;
	}
}
Frame.prototype.delete = function(id) {
	if(id == undefined) {
		this.close();
		if(this.__relations__.parent) {
			this.__relations__.parent.delete(this.id);
		}
	}
	else {
		for(var c=0;c<this.__relations__.child.length;c++) {
			if(this.__relations__.child[c].id == id) {
				this.__relations__.child.splice(c--,1);
				break;
			}
		}
	}
}
Frame.prototype.move = function(x,y) {
	var nx = undefined;
	var ny = undefined;
	if(this.__checkX__(this.x+x) && this.__checkWidth__(this.x+x,this.width))
		nx = this.x+x;
	if(this.__checkY__(this.y+y) && this.__checkHeight__(this.y+y,this.height))
		ny = this.y+y;
	if(nx == undefined && ny == undefined)
		return;
	this.__properties__.display.updateFrame(this);
	if(nx !== undefined)
		this.x=nx;
	if(ny !== undefined)
		this.y=ny;
	this.__properties__.display.updateFrame(this);
	for each(var c in this.__relations__.child) 
		c.move(x,y);
}
Frame.prototype.moveTo = function(x,y) {
	for each(var c in this.__relations__.child) {
		var cx = (x + (c.x-this.x));
		var cy = (y + (c.y-this.y));
		c.moveTo(cx,cy);
	}
	var nx = undefined;
	var ny = undefined;
	if(this.__checkX__(x))
		nx = x;
	if(this.__checkY__(y))
		ny = y;
	if(nx == undefined && ny == undefined)
		return;
	this.__properties__.display.updateFrame(this);
	if(nx !== undefined)
		this.x=nx;
	if(ny !== undefined)
		this.y=ny;
	this.__properties__.display.updateFrame(this);
}
Frame.prototype.draw = function() {
	if(this.__properties__.open)
		this.refresh();
	else
		this.open();
	this.cycle();
}
Frame.prototype.cycle = function() {
	return this.__properties__.display.cycle();
}
Frame.prototype.load = function(filename,width,height) {
	var f=new File(filename);
	if (!f.open("rb", true))
		return false;
	var contents=f.read();
	f.close();
	var valid_sauce = false;
	var ext = file_getext(filename).substr(1).toUpperCase();

	if (contents.substr(-128, 7) == "SAUCE00") {
		var sauceless_size = ascii(contents.substr(-35,1));
		sauceless_size <<= 8;
		sauceless_size |= ascii(contents.substr(-36,1));
		sauceless_size <<= 8;
		sauceless_size |= ascii(contents.substr(-37,1));
		sauceless_size <<= 8;
		sauceless_size |= ascii(contents.substr(-38,1));

		var data_type = ascii(contents.substr(-34,1));
		var file_type = ascii(contents.substr(-33,1));
		var tinfo1 = ascii(contents.substr(-31,1));
		tinfo1 <<= 8;
		tinfo1 |= ascii(contents.substr(-32,1));
		var tinfo2 = ascii(contents.substr(-29,1));
		tinfo2 <<= 8;
		tinfo2 |= ascii(contents.substr(-30,1));
		switch(data_type) {
			case 1:
				switch(file_type) {
					case 0:	// PLain ASCII
						ext = "TXT";
						if (tinfo1)
							width = tinfo1;
						if (tinfo2)
							height = tinfo2;
						break;
					case 1: // ANSi
						ext = "ANS";
						if (tinfo1)
							width = tinfo1;
						if (tinfo2)
							height = tinfo2;
						break;
					case 7: // Source
						ext = "TXT";
						break;
				}
				valid_sauce = true;
				break;
			case 5:
				ext = 'BIN';
				width = file_type * 2;
				height = (sauceless_size / 2) / width;
				valid_sauce = true;
				break;
		}
		if (valid_sauce)
			contents = contents.substr(0, sauceless_size);
	}

	switch(ext) {
	case "ANS":
		/*
		 * TODO: This doesn't do exactly what reading a text file does
		 * one Windows (nor on Linux), but it should be close to what
		 * was meant, and should work given that it resets x every line.
		 */
		var lines=contents.split(/\r\n/);

		var attr = this.attr;
		var bg = BG_BLACK;
		var fg = LIGHTGRAY;

		var i = 0;
		var y = 0;
		var saved = {};
		
		while(lines.length > 0) {	
			var x = 0;
			var line = lines.shift();
			/* parse 'ATCODES'?? 
			line = line.replace(/@(.*)@/g,
				function (str, code, offset, s) {
					return bbs.atcode(code);
				}
			);
			*/
			while(line.length > 0) {
				/* parse an attribute sequence*/
				var m = line.match(/^\x1b\[(\d*);?(\d*);?(\d*)m/);
				if(m !== null) {
					line = line.substr(m.shift().length);
					if(m[0] == 0) {
						bg = BG_BLACK;
						fg = LIGHTGRAY;
						i = 0;
						m.shift();
					}
					if(m[0] == 1) {
						i = HIGH;
						m.shift();
					}
					if(m[0] >= 40) {
						switch(Number(m.shift())) {
						case 40:
							bg = BG_BLACK;
							break;
						case 41:
							bg = BG_RED;
							break;
						case 42: 
							bg = BG_GREEN;
							break;
						case 43:
							bg = BG_BROWN;
							break;
						case 44:
							bg = BG_BLUE;
							break;
						case 45:
							bg = BG_MAGENTA;
							break;
						case 46:
							bg = BG_CYAN;
							break;
						case 47:
							bg = BG_LIGHTGRAY;
							break;
						}
					}
					if(m[0] >= 30) {
						switch(Number(m.shift())) {
						case 30:
							fg = BLACK;
							break;
						case 31:
							fg = RED;
							break;
						case 32:
							fg = GREEN;
							break;
						case 33:
							fg = BROWN;
							break;
						case 34:
							fg = BLUE;
							break;
						case 35:
							fg = MAGENTA;
							break;
						case 36:
							fg = CYAN;
							break;
						case 37:
							fg = LIGHTGRAY;
							break;
						}
					}
					attr = bg + fg + i;
					continue;
				}
				
				/* parse absolute character position */
				var m = line.match(/^\x1b\[(\d*);?(\d*)[Hf]/);
				if(m !== null) {
					line = line.substr(m.shift().length);
					
					if(m.length==0) {
						x=0;
						y=0;
					}
					else {
						if(m[0])
							y = Number(m.shift())-1;
						if(m[0])
							x = Number(m.shift())-1;
					}
					continue;
				}
				
				/* ignore a bullshit sequence */
				var n = line.match(/^\x1b\[\?7h/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					continue;
				}
				
				/* parse an up positional sequence */
				var n = line.match(/^\x1b\[(\d*)A/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					var chars = n.shift();
					if(chars < 1)
						y-=1;
					else
						y-=Number(chars);
					continue;
				}
				
				/* parse a down positional sequence */
				var n = line.match(/^\x1b\[(\d*)B/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					var chars = n.shift();
					if(chars < 1)
						y+=1;
					else
						y+=Number(chars);
					continue;
				}
				
				/* parse a forward positional sequence */
				var n = line.match(/^\x1b\[(\d*)C/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					var chars = n.shift();
					if(chars < 1)
						x+=1;
					else
						x+=Number(chars);
					continue;
				}
				
				/* parse a backward positional sequence */
				var n = line.match(/^\x1b\[(\d*)D/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					var chars = n.shift()
					if(chars < 1)
						x-=1;
					else
						x-=Number(chars);
					continue;
				}
				
				/* parse a clear screen sequence */
				var n = line.match(/^\x1b\[2J/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					continue;
				}
				
				/* parse save cursor sequence */
				var n = line.match(/^\x1b\[s/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					saved.x = x;
					saved.y = y;
					continue;
				}

				/* parse restore cursor sequence */
				var n = line.match(/^\x1b\[u/);
				if(n !== null) {
					line = line.substr(n.shift().length);
					x = saved.x;
					y = saved.y;
					continue;
				}

				/* set character and attribute */
				var ch = line[0];
				line = line.substr(1);
				
				/* validate position */
				if(y<0) 
					y=0; 
				if(x<0)
					x=0;
				if(x>=this.width) {
					x=0;
					y++;
				}
				/* set character and attribute */
				if(!this.__properties__.data[y])
					this.__properties__.data[y]=[];
				this.__properties__.data[y][x]=new Char(ch,attr);
				x++;
			}
			y++;
		}			
		break;
    case "BIN":
        if (!this.load_bin(contents, width, height, 0)) return false;
		break;
	case "TXT":
		var lines=contents.split(/\r\n/);
		while(lines.length > 0)
			this.putmsg(lines.shift() + "\r\n");
		break;
	default:
		throw("unsupported filetype");
		break;
	}
}
Frame.prototype.load_bin = function(contents, width, height, offset) {
    if(width == undefined || height == undefined)
        throw("unknown graphic dimensions");
    if(offset == undefined) offset = 0;
    for(var y=0; y<height; y++) {
        for(var x=0; x<width; x++) {
            var c = new Char();
            if(offset >= contents.length)
                return(false);
            c.ch = contents.substr(offset++, 1);
            if(offset == contents.length)
                return(false);
            c.attr = ascii(contents.substr(offset++, 1));
            c.id = this.id;
            if(!this.__properties__.data[y])
                this.__properties__.data[y]=[];
            this.__properties__.data[y][x] = c;
        }
    }
    return true;
}
Frame.prototype.scroll = function(x,y) {
	var update = false;
	/* default: add a new line to the data matrix */
	if(x == undefined && y == undefined) {
		if(this.__settings__.v_scroll) {
			var newrow = [];
			for(var x = 0;x<this.width;x++) {
				for(var y = 0;y<this.height;y++) 
					this.__properties__.display.updateChar(this,x,y);
				newrow.push(new Char());
			}
			this.__properties__.data.push(newrow);
			this.__position__.offset.y++;
			update = true;
		}
	}
	/* otherwise, adjust the x/y offset */
	else {
		if(typeof x == "number" && x !== 0 && this.__settings__.h_scroll) {
			this.__position__.offset.x += x;
			update = true;
		}
		if(typeof y == "number" && y !== 0 && this.__settings__.v_scroll) {
			this.__position__.offset.y += y;
			update = true;
		}
		if(update)
			this.refresh();
	}
	return update;
}
Frame.prototype.scrollTo = function(x,y) {
	var update = false;
	if(typeof x == "number") {
		if(this.__settings__.h_scroll) {
			this.__position__.offset.x = x;
			update = true;
		}
	}
	if(typeof y == "number") {
		if(this.__settings__.v_scroll) {
			this.__position__.offset.y = y;
			update = true;
		}
	}
	if(update)
		this.refresh();
}
Frame.prototype.insertLine = function(y) {
	if(this.__properties__.data[y])
		this.__properties__.data.splice(y,0,[]);
	else
		this.__properties__.data[y] = [];
	this.refresh();
	return this.__properties__.data[y];
}
Frame.prototype.deleteLine = function(y) {
	var l = undefined;
	if(this.__properties__.data[y]) {
		l = this.__properties__.data.splice(y,1);
		this.refresh();
	}
	return l;
}
Frame.prototype.screenShot = function(file,append) {
	return this.__properties__.display.screenShot(file,append);
}
Frame.prototype.invalidate = function() {
	this.__properties__.display.invalidate();
	this.refresh();
}
Frame.prototype.dump = function() {
	return this.__properties__.display.dump();
}

/* console method emulation */
Frame.prototype.home = function() {
	this.__position__.cursor.x = 0;
	this.__position__.cursor.y = 0;
	return true;
}
Frame.prototype.end = function() {
	this.__position__.cursor.x = this.width-1;
	this.__position__.cursor.y = this.height-1;
	return true;
}
Frame.prototype.pagedown = function() {
	this.__position__.offset.y += this.height-1;
	if(this.__position__.offset.y >= this.data_height)
		this.__position__.offset.y = this.data_height - this.height;
	this.refresh();
}
Frame.prototype.pageup = function() {
	this.__position__.offset.y -= this.height-1;
	if(this.__position__.offset.y < 0)
		this.__position__.offset.y = 0;
	this.refresh();
}
Frame.prototype.clear = function (attr) {
	if (attr) this.attr = attr;
	this.__properties__.data = [];
	this.__position__.offset.x = 0;
	this.__position__.offset.y = 0;
	this.home();
	this.invalidate();
}
Frame.prototype.clearline = function(attr) {
	if(attr == undefined)
		attr = this.attr;
	var py = this.__position__.offset.y + this.__position__.cursor.y;
	if(!this.__properties__.data[py])
		return false;
	for(var x=0;x<this.__properties__.data[py].length;x++) {
		if(this.__properties__.data[py][x]) {
			this.__properties__.data[py][x].ch = undefined;
			this.__properties__.data[py][x].attr = attr;
		}
	}
	for(var x=0;x<this.width;x++) {
		this.__properties__.display.updateChar(this,x,this.__position__.cursor.y);
	}
}
Frame.prototype.cleartoeol = function(attr) {
	if(attr == undefined)
		attr = this.attr;
	var px = this.__position__.offset.x + this.__position__.cursor.x;
	var py = this.__position__.offset.y + this.__position__.cursor.y;
	if(!this.__properties__.data[this.__position__.cursor.y])
		return false;
	for(var x=px;x<this.__properties__.data[py].length;x++) {
		if(this.__properties__.data[py][x]) {
			this.__properties__.data[py][x].ch = undefined;
			this.__properties__.data[py][x].attr = attr;
		}
	}
	for(var x=this.__position__.cursor.x;x<this.width;x++) {
		this.__properties__.display.updateChar(this,x,this.__position__.cursor.y);
	}
}
Frame.prototype.crlf = function() {
	this.__position__.cursor.x = 0;
	if(this.__position__.cursor.y < this.height-1) 
		this.__position__.cursor.y += 1;
	else {}
}
Frame.prototype.write = function(str,attr) {
	if(str == undefined)
		return;
	if(this.__settings__.word_wrap) 
		str = word_wrap(str, this.width,str.length, false);
	str = str.toString().split('');
	
	if(attr)
		this.__properties__.curr_attr = attr;
	else
		this.__properties__.curr_attr = this.attr;
	var pos = this.__position__.cursor;
	while(str.length > 0) {
		var ch = str.shift();
		this.__putChar__.call(this,ch,this.__properties__.curr_attr);
		pos.x++;
	}
}
Frame.prototype.putmsg = function(str,attr) {
	if(str == undefined)
		return;
	if(this.__settings__.word_wrap) {
		var remainingWidth = this.width - this.__position__.cursor.x;
		if(str.length > remainingWidth) {
			str = word_wrap(str,remainingWidth,str.length,false).split('\n');
			str = str.shift() + '\r\n' + word_wrap(str.join('\r\n'),this.width,remainingWidth,false);
		}
	}
	str = str.toString().split('');
	
	if(attr)
		this.__properties__.curr_attr = attr;
	else
		this.__properties__.curr_attr = this.attr;
	var pos = this.__position__.cursor;

	while(str.length > 0) {
		var ch = str.shift();
		if(this.__properties__.ctrl_a) {
			var k = ch;
			if(k)
				k = k.toUpperCase();
			switch(k) {
			case '\1':	/* A "real" ^A code */
				this.__putChar__.call(this,ch,this.__properties__.curr_attr);
				pos.x++;
				break;
			case 'K':	/* Black */
				this.__properties__.curr_attr=(this.__properties__.curr_attr)&0xf8;
				break;
			case 'R':	/* Red */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0xf8)|RED;
				break;
			case 'G':	/* Green */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0xf8)|GREEN;
				break;
			case 'Y':	/* Yellow */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0xf8)|BROWN;
				break;
			case 'B':	/* Blue */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0xf8)|BLUE;
				break;
			case 'M':	/* Magenta */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0xf8)|MAGENTA;
				break;
			case 'C':	/* Cyan */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0xf8)|CYAN;
				break;
			case 'W':	/* White */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0xf8)|LIGHTGRAY;
				break;
			case '0':	/* Black */
				this.__properties__.curr_attr=(this.__properties__.curr_attr)&0x8f;
				break;
			case '1':	/* Red */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0x8f)|(RED<<4);
				break;
			case '2':	/* Green */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0x8f)|(GREEN<<4);
				break;
			case '3':	/* Yellow */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0x8f)|(BROWN<<4);
				break;
			case '4':	/* Blue */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0x8f)|(BLUE<<4);
				break;
			case '5':	/* Magenta */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0x8f)|(MAGENTA<<4);
				break;
			case '6':	/* Cyan */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0x8f)|(CYAN<<4);
				break;
			case '7':	/* White */
				this.__properties__.curr_attr=((this.__properties__.curr_attr)&0x8f)|(LIGHTGRAY<<4);
				break;
			case 'H':	/* High Intensity */
				this.__properties__.curr_attr|=HIGH;
				break;
			case 'I':	/* Blink */
				this.__properties__.curr_attr|=BLINK;
				break;
			case 'N': 	/* Normal */
				this.__properties__.curr_attr&=~HIGH;
				this.__properties__.curr_attr&=~BLINK;
				break;
			case '-':	/* Normal if High, Blink, or BG */
				if(this.__properties__.curr_attr & 0xf8)
					this.__properties__.curr_attr=this.attr;
				break;
			case '_':	/* Normal if blink/background */
				if(this.__properties__.curr_attr & 0xf0)
					this.__properties__.curr_attr=this.attr;
				break;
			case '[':	/* CR */
				pos.x=0;
				break;
			case ']':	/* LF */
				pos.y++;
				if(this.__settings__.lf_strict && pos.y >= this.height) {	
					this.scroll();
					pos.y--;
				}
				break;
			default:	/* Other stuff... specifically, check for right movement */
				if(ch.charCodeAt(0)>127) 
					pos.x+=ch.charCodeAt(0)-127;
				break;
			}
			this.__properties__.ctrl_a = false;
		}
		else {
			switch(ch) {
			case '\1':		/* CTRL-A code */
			case ctrl('A'):
				this.__properties__.ctrl_a = true;
				break;
			case '\7':		/* Beep */
				break;
			case '\x7f':	/* DELETE */
				break;
			case '\b':
				if(pos.x > 0) {
					pos.x--;
					this.__putChar__.call(this," ",this.__properties__.curr_attr);
				}
				break;
			case '\r':
				pos.x=0;
				break;
			case '\t':
				pos.x += 4;
				break;
			case '\n':
				pos.y++;
				if(this.__settings__.lf_strict && pos.y >= this.height) {	
					this.scroll();
					pos.y--;
				}
				break;
			default:
				this.__putChar__.call(this,ch,this.__properties__.curr_attr);
				pos.x++;
				break;
			}
		}
	}
}
Frame.prototype.center = function(str,attr) {
	this.__position__.cursor.x = Math.ceil(this.width/2) - Math.ceil(console.strlen(strip_ctrl(str))/2);
	if(this.__position__.cursor.x < 0)
		this.__position__.cursor.x = 0;
	this.putmsg(str,attr);
}
Frame.prototype.gotoxy = function(x,y) {
	if(typeof x == "object" && x.x && x.y) {
		if(this.__validateCursor__.call(this,x.x-1,x.y-1)) {
			this.__position__.cursor.x = x.x-1;
			this.__position__.cursor.y = x.y-1;
			return true;
		}
		return false;
	}
	if(this.__validateCursor__.call(this,x-1,y-1)) {
		this.__position__.cursor.x = x-1;
		this.__position__.cursor.y = y-1;
		return true;
	}
	return false;
}
Frame.prototype.getxy = function() {
	return {
		x:this.__position__.cursor.x+1,
		y:this.__position__.cursor.y+1
	};
}
Frame.prototype.pushxy = function() {
	this.__position__.stored.x = this.__position__.cursor.x;
	this.__position__.stored.y = this.__position__.cursor.y;
}
Frame.prototype.popxy = function() {
	this.__position__.cursor.x = this.__position__.stored.x;
	this.__position__.cursor.y = this.__position__.stored.y;
}
Frame.prototype.up = function(n) {
	if(this.__position__.cursor.y == 0 && this.__position__.offset.y == 0)
		return false;
	if(isNaN(n))
		n = 1;
	while(n > 0) {
		if(this.__position__.cursor.y > 0) {
			this.__position__.cursor.y--;
			n--;
		}
		else break;
	}
	if(n > 0) 
		this.scroll(0,-(n));
	return true;
}
Frame.prototype.down = function(n) {
	if(this.__position__.cursor.y == this.height-1 && this.__position__.offset.y == this.data_height - this.height)
		return false;
	if(isNaN(n))
		n = 1;
	while(n > 0) {
		if(this.__position__.cursor.y < this.height - 1) {
			this.__position__.cursor.y++;
			n--;
		}
		else break;
	}
	if(n > 0)
		this.scroll(0,n);
	return true;
}
Frame.prototype.left = function(n) {
	if(this.__position__.cursor.x == 0 && this.__position__.offset.x == 0)
		return false;
	if(isNaN(n))
		n = 1;
	while(n > 0) {
		if(this.__position__.cursor.x > 0) {
			this.__position__.cursor.x--;
			n--;
		}
		else break;
	}
	if(n > 0) 
		this.scroll(-(n),0);
	return true;
}
Frame.prototype.right = function(n) {
	if(this.__position__.cursor.x == this.width-1 && this.__position__.offset.x == this.data_width - this.width)
		return false;
	if(isNaN(n))
		n = 1;
	while(n > 0) {
		if(this.__position__.cursor.x < this.width - 1) {
			this.__position__.cursor.x++;
			n--;
		}
		else break;
	}
	if(n > 0) 
		this.scroll(n,0);
	return true;
}

/* internal frame methods */
Frame.prototype.__checkX__ = function(x) {
	if(	isNaN(x) || (this.__settings__.checkbounds &&  
		(x > this.__properties__.display.x + this.__properties__.display.width || 
		x < this.__properties__.display.x)))
		return false;
	return true;
}
Frame.prototype.__checkY__ = function(y) {
	if( isNaN(y) || (this.__settings__.checkbounds && 
		(y > this.__properties__.display.y + this.__properties__.display.height || 
		y < this.__properties__.display.y)))
		return false;
	return true;
}
Frame.prototype.__checkWidth__ = function(x,width) {
	if(	width < 1 || isNaN(width) || (this.__settings__.checkbounds && 
		x + width > this.__properties__.display.x + this.__properties__.display.width))
		return false;
	return true;
}
Frame.prototype.__checkHeight__ = function(y,height) {
	if( height < 1 || isNaN(height) || (this.__settings__.checkbounds && 
		y + height > this.__properties__.display.y + this.__properties__.display.height))
		return false;
	return true;
}
Frame.prototype.__validateCursor__ = function(x,y) {
	if(x >= this.width) 
		return false;
	if(y >= this.height) 	
		return false;
	return true;
}
Frame.prototype.__putChar__ = function(ch,attr) {
	if(this.__position__.cursor.x >= this.width) {
		this.__position__.cursor.x=0;
		this.__position__.cursor.y++;
	}
	if(this.__position__.cursor.y >= this.height) {	
		this.scroll();
		this.__position__.cursor.y--;
	}
	this.setData(this.__position__.cursor.x,this.__position__.cursor.y,ch,attr,true);
}

Frame.prototype.__flip__ = function (swaps) {
    for (var y = 0; y < this.data_height; y++) {
        if (!Array.isArray(this.data[y])) continue;
        this.data[y] = this.data[y].reverse();
        for (var x = 0; x < this.data[y].length; x++) {
            for (var s = 0; s < swaps.length; s++) {
                var i = swaps[s].indexOf(ascii(this.data[y][x].ch));
                if (i >= 0) {
                    this.data[y][x].ch = ascii(swaps[s][(i + 1) % 2]);
                    break;
                }
            }
        }
    }
}

Frame.prototype.flipX = function () {
    const swaps = [
        [ 40, 41 ],
        [ 47, 92 ],
        [ 60, 62 ],
        [ 91, 93 ],
        [ 123, 125 ],
        [ 169, 170 ],
        [ 174, 175 ],
        [ 180, 195 ],
        [ 181, 198 ],
        [ 182, 199 ],
        [ 183, 214 ],
        [ 184, 213 ],
        [ 185, 204 ],
        [ 187, 201 ],
        [ 188, 200 ],
        [ 189, 211 ],
        [ 190, 212 ],
        [ 191, 218 ],
        [ 192, 217 ],
        [ 221, 222 ]
    ];
    this.__flip__(swaps);
}

Frame.prototype.flipY = function () {
    const swaps = [
        [ 47, 92 ],
        [ 183, 189 ],
        [ 184, 190 ],
        [ 187, 188 ],
        [ 191, 217 ],
        [ 192, 218 ],
        [ 193, 194 ],
        [ 200, 201 ],
        [ 202, 203 ],
        [ 207, 209 ],
        [ 208, 210 ],
        [ 211, 214 ],
        [ 212, 213 ],
        [ 220, 223 ]
    ];
    this.__flip__(swaps);
    this.data.reverse();
}

/* frame reference object */
function Canvas(frame,display) {
	this.frame = frame;
	this.display = display;
	this.__defineGetter__("xoff",function() {
		return this.frame.x - this.display.x;
	});
	this.__defineGetter__("yoff",function() {
		return this.frame.y - this.display.y;
	});
}

Canvas.prototype.hasData = function(x,y) {
	var xpos = x-this.xoff;
	var ypos = y-this.yoff;
	
	if(xpos < 0 || ypos < 0)
		return undefined;
	if(xpos >= this.frame.width || ypos >= this.frame.height)
		return undefined;
	if(this.frame.transparent && this.frame.getData(xpos,ypos,true).ch == undefined)
		return undefined;
	return true;
}

/* object representing screen positional and dimensional limits and canvas stack */
function Display(x,y,width,height) {
	/* private properties */
	this.__properties__ = {
		x:undefined,
		y:undefined,
		width:undefined,
		height:undefined,
		canvas:{},
		update:{},
		buffer:{},
		count:0
	}

	/* initialize display properties */
	this.x = x;
	this.y = y;
	this.width = width;
	this.height = height;
	log(LOG_DEBUG,format("new display initialized: %sx%s at %s,%s",this.width,this.height,this.x,this.y));
}

/* protected display properties */
Display.prototype.__defineGetter__("x", function() {
	return this.__properties__.x;
});
Display.prototype.__defineSetter__("x", function(x) {
	if(x == undefined)
		this.__properties__.x = 1;
	else if(isNaN(x))
		throw("invalid x coordinate: " + x);
	else 
		this.__properties__.x = Number(x);
});
Display.prototype.__defineGetter__("y", function() {
	return this.__properties__.y;
});
Display.prototype.__defineSetter__("y", function(y) {
	if(y == undefined)
		this.__properties__.y = 1;
	else if(isNaN(y) || y < 1 || y > console.screen_rows)
		throw("invalid y coordinate: " + y);
	else 
		this.__properties__.y = Number(y);
});
Display.prototype.__defineGetter__("width", function() {
	return this.__properties__.width;
});
Display.prototype.__defineSetter__("width", function(width) {
	if(width == undefined)
		this.__properties__.width = console.screen_columns;
	else if(isNaN(width) || (this.x + Number(width) - 1) > (console.screen_columns))
		throw("invalid width: " + width);
	else 
		this.__properties__.width = Number(width);
});
Display.prototype.__defineGetter__("height", function() {
	return this.__properties__.height;
});
Display.prototype.__defineSetter__("height", function(height) {
	if(height == undefined)
		this.__properties__.height = console.screen_rows;
	else if(isNaN(height) || (this.y + Number(height) - 1) > (console.screen_rows))
		throw("invalid height: " + height);
	else
		this.__properties__.height = Number(height);
});
Display.prototype.__defineGetter__("nextID",function() {
	return ++this.__properties__.count;
});

/* public display methods */
Display.prototype.cycle = function() {
	var updates = this.__getUpdateList__();
	if(updates.length > 0) {
		var lasty = undefined;
		var lastx = undefined;
		var lastf = undefined;
		for each(var u in updates) {
			if(lasty !== u.y || lastx == undefined || (u.x - lastx) !== 1)
				console.gotoxy(u.px,u.py);
			if(lastf !== u.id)
				console.attributes = undefined;
			this.__drawChar__(u.ch,u.attr,u.px,u.py);
			lastx = u.x;
			lasty = u.y;
			lastf = u.id;
		}
		//console.attributes=undefined;
		return true;
	}
	return false;
}
Display.prototype.draw = function() {
	for(var y = 0;y<this.height;y++) {
		for(var x = 0;x<this.width;x++) {
			this.__updateChar__(x,y);
		}
	}
	this.cycle();
}
Display.prototype.open = function(frame) {
	var canvas = new Canvas(frame,this);
	this.__properties__.canvas[frame.id] = canvas;
	this.updateFrame(frame);
}
Display.prototype.close = function(frame) {
	this.updateFrame(frame);
	delete this.__properties__.canvas[frame.id];
}
Display.prototype.top = function(frame) {
	var canvas = this.__properties__.canvas[frame.id];
	delete this.__properties__.canvas[frame.id];
	this.__properties__.canvas[frame.id] = canvas;
	this.updateFrame(frame);
}
Display.prototype.bottom = function(frame) {
	for(var c in this.__properties__.canvas) {
		if(c == frame.id)
			continue;
		var canvas = this.__properties__.canvas[c];
		delete this.__properties__.canvas[c];
		this.__properties__.canvas[c] = canvas;
	}
	this.updateFrame(frame);
}
Display.prototype.updateFrame = function(frame) {
	for(var y = 0;y<frame.height;y++) {
		for(var x = 0;x<frame.width;x++) {
			this.updateChar(frame,x,y/*,data*/);
		}
	}
}
Display.prototype.updateChar = function(frame,x,y) {
	var xoff = frame.x - this.x;
	var yoff = frame.y - this.y;
	this.__updateChar__(xoff + x,yoff + y);
}
Display.prototype.screenShot = function(file,append) {
	var f = new File(file);
	if(append) 
		f.open('ab',true,4096);
	else
		f.open('wb',true,4096) ;
	if(!f.is_open)
		return false;

	for(var y = 0;y<this.height;y++) {
		for(var x = 0;x<this.width;x++) {
			var c = this.__getTopCanvas__(x,y);
			var d = this.__getData__(c,x,y);
			if(d.ch)
				f.write(d.ch);
			else
				f.write(" ");
			if(d.attr)
				f.writeBin(d.attr,1);
			else
				f.writeBin(0,1);
		}
	}
	
	f.close();
	return true;
}
Display.prototype.invalidate = function() {
	this.__properties__.buffer = {};
}
Display.prototype.dump = function() {
	var arr = [];
	for(var y = 0; y < this.height; y++) {
		arr[y] = [];
		for(var x = 0; x < this.width; x++) {
			var c = this.__getTopCanvas__(x, y);
			if(typeof c == "undefined")
				continue;
			var d = this.__getData__(c, x, y);
			if(typeof d.ch != "undefined")
				arr[y][x] = d;
		}
	}
	return arr;
}

/* private functions */
Display.prototype.__updateChar__ = function(x,y/*,data*/) {
	//ToDo: maybe store char update so I dont have to retrieve it later?
	if(!this.__properties__.update[y])
		this.__properties__.update[y] = {};
	this.__properties__.update[y][x] = 1; /*data; */
}
Display.prototype.__getUpdateList__ = function() {
	var list = [];
	for(var y in this.__properties__.update) {
		for(var x in this.__properties__.update[y]) {
			var c = this.__getTopCanvas__(x,y);
			var d = this.__getData__(c,x,y);
			
			if(d.px < 1 ||  d.py < 1 || d.px > console.screen_columns || d.py > console.screen_rows)
				continue;
			if(!this.__properties__.buffer[x]) {
				this.__properties__.buffer[x] = {};
				this.__properties__.buffer[x][y] = d;
				list.push(d);
			}
			else if(this.__properties__.buffer[x][y] == undefined ||
				this.__properties__.buffer[x][y].ch != d.ch || 
				this.__properties__.buffer[x][y].attr != d.attr) {
				this.__properties__.buffer[x][y] = d;
				list.push(d);
			}
		}
	}
	this.__properties__.update={};
	return list.sort(function(a,b) {
		if(a.y == b.y)
			return a.x-b.x;
		return a.y-b.y;
	});
}
Display.prototype.__getData__ = function(c,x,y) {
	var cd = {
		x:Number(x),
		y:Number(y),
		px:Number(x) + this.__properties__.x,
		py:Number(y) + this.__properties__.y
	};
	if(c) {
		var d = c.frame.getData(x-c.xoff,y-c.yoff,true);
		cd.id = c.frame.id;
		cd.ch = d.ch;
		if(d.attr)
			cd.attr = d.attr;
		else
			cd.attr = c.frame.attr;
	}
	return cd;
}
Display.prototype.__drawChar__ = function(ch,attr,xpos,ypos) {
	if(attr)
		console.attributes = attr;
	if(xpos == console.screen_columns && ypos == console.screen_rows) 
		console.cleartoeol();
	else if(ch == undefined) {
		console.write(" ");
	}	
	else {
		console.write(ch);
	}
}
Display.prototype.__getTopCanvas__ = function(x,y) {
	var top = undefined;
	for each(var c in this.__properties__.canvas) {
		if(c.hasData(x,y))
			top = c;
	}
	return top;
}

/* character/attribute pair representing a screen position and its contents */
function Char(ch,attr) {
	this.ch = ch;
	this.attr = attr;
}

/* self-validating cursor position object */
function Cursor(x,y,frame) {
	this.__properties__ = {
		x:undefined,
		y:undefined,
		frame:undefined
	}
	this.__defineGetter__("x", function() { 
		return this.__properties__.x;
	});
	this.__defineSetter__("x", function(x) {
		if(isNaN(x))
			throw("invalid x coordinate: " + x);
		this.__properties__.x = x;
	});
	this.__defineGetter__("y", function() { 
		return this.__properties__.y;
	});
	this.__defineSetter__("y", function(y) {
		if(isNaN(y))
			throw("invalid y coordinate: " + y);
		this.__properties__.y = y;
	});
	
	if(frame instanceof Frame)
		this.__properties__.frame = frame;
	else
		throw("the frame is not a frame");
		
	this.x = x;
	this.y = y;
}

/* self-validating scroll offset object */
function Offset(x,y,frame) {
	this.__properties__ = {
		x:undefined,
		y:undefined,
		frame:undefined
	}
	this.__defineGetter__("x", function() { 
		return this.__properties__.x;
	});
	this.__defineSetter__("x", function(x) {
		if(x == undefined)
			throw("invalid x offset: " + x);
		else if(x < 0)
			x = 0;
/* 		else if(x > this.__properties__.frame.data_width - this.__properties__.frame.width)	
			x = this.__properties__.frame.data_width - this.__properties__.frame.width;
 */		this.__properties__.x = x;
	});
	this.__defineGetter__("y", function() { 
		return this.__properties__.y;
	});
	this.__defineSetter__("y", function(y) {
		if(y == undefined)
			throw("invalid y offset: " + y);
		else if(y < 0)
			y = 0;
/* 		else if(y > this.__properties__.frame.data_height - this.__properties__.frame.height)
			y = this.__properties__.frame.data_height - this.__properties__.frame.height;
 */		this.__properties__.y = y;
	});
	
	if(frame instanceof Frame)
		this.__properties__.frame = frame;
	else
		throw("the frame is not a frame");
		
	this.x = x;
	this.y = y;
}

