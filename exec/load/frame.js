/* $Id$ */
/**
 	Javascript Frame Library 					
 	for Synchronet v3.15a+ 
 	by Matt Johnson (2011)	

 DESCRIPTION:

 	this library is meant to be used in conjunction with other libraries that
 	store display data in a Frame() object or objects
 	this allows for "windows" that can be hidden and closed without 
 	destroying the data behind them.

 	the object itself takes the following parameters:

 		x: 			the coordinate representing the top left corner of the frame (horiz)
 		y: 			the coordinate representing the top left corner of the frame (vert)
 		width: 		the horizontal width of the frame 
 		height: 	the vertical height of the frame
 		attr:		the background color of the frame (displayed when there are no other contents)
		
METHODS:

	frame.open();				//populates frame contents in character canvas
	frame.close();				//removes frame contents from character canvas
	frame.draw();				//draws the characters occupied by 'frame' coords/dimensions 
	frame.cycle();				//checks the display matrix for updated characters and displays them 
	frame.load(filename):		//loads a binary graphic (.BIN) or ANSI graphic (.ANS) file
	frame.bottom();				//push frame to bottom of display stack
	frame.top();				//pull frame to top of display stack
	frame.scroll(dir);			//scroll frame one line in either direction ***NOT YET IMPLEMENTED***
	frame.move(x,y);			//move frame one space where x = -1,0,1 and y = -1,0,1 
	frame.moveTo(x,y);			//move frame to absolute position
	frame.clearline(attr);		//see http://synchro.net/docs/jsobjs.html#console
	frame.cleartoeol(attr);
	frame.putmsg(str);
	frame.clear(attr);
	frame.home();
	frame.center(str);
	frame.crlf();
	frame.getxy();
	frame.gotoxy(x,y);

 USAGE:

	//create a new frame object at screen position 1,1. 80 characters wide by 24 tall
	var frame = load("frame.js",1,1,80,24,BG_BLUE);
	
 	//or it can be done this way.....
 	load("frame.js");
 	var frame = new Frame(1,1,80,24,BG_BLUE);
 
	//add a new frame within the frame object that will display on top at position 10,10
	var subframe = new Frame(10,10,10,10,BG_GREEN,frame);
	
	//beware this sample infinite loop
 	while(!js.terminated) { 
	
		//on first call this will draw the entire initial frame
		//on subsequent calls this will draw only characters that have changed
		frame.cycle();
	}
	
	//close out the entire frame tree
	frame.close();
	
TODO:

	add a named Queue() to optionally receive messages from outside of
	the parent script
	
 */
 
load("sbbsdefs.js");

function Frame(x,y,width,height,attr,frame) {

	/* object containing data matrix and frame pointer */
	function Canvas(frame,display) {
		this.frame = frame;
		this.display = display;
		this.__defineGetter__("xoff",function() {
			return this.frame.x - this.display.x;
		});
		this.__defineGetter__("yoff",function() {
			return this.frame.y - this.display.y;
		});
		this.getData = function(x,y) {
			if(x - this.xoff < 0 || y - this.yoff < 0)
				return undefined;
			if(x - this.xoff >= frame.width || y - this.yoff >= frame.height)
				return undefined;
			return frame.getData(x-this.xoff,y-this.yoff);
		}
	}
	
	/* object representing frame positional and dimensional limits and canvas stack */
	function Display(x,y,width,height) {
		/* private properties */
		var properties = {
			x:undefined,
			y:undefined,
			width:undefined,
			height:undefined,
			canvas:{},
			update:{}
		}

		/* protected properties */
		this.__defineGetter__("x", function() {
			return properties.x;
		});
		this.__defineSetter__("x", function(x) {
			if(x == undefined)
				properties.x = 1;
			else if(isNaN(x) || x < 1 || x > console.screen_columns)
				throw("invalid x coordinate: " + x);
			else 
				properties.x = x;
		});
		this.__defineGetter__("y", function() {
			return properties.y;
		});
		this.__defineSetter__("y", function(y) {
			if(y == undefined)
				properties.y = 1;
			else if(isNaN(y) || y < 1 || y > console.screen_rows)
				throw("invalid y coordinate: " + y);
			else 
				properties.y = y;
		});
		this.__defineGetter__("width", function() {
			return properties.width;
		});
		this.__defineSetter__("width", function(width) {
			if(width == undefined)
				properties.width = console.screen_columns;
			else if(isNaN(width) || (x + width - 1) > (console.screen_columns))
				throw("invalid width: " + width);
			else 
				properties.width = width;
		});
		this.__defineGetter__("height", function() {
			return properties.height;
		});
		this.__defineSetter__("height", function(height) {
			if(height == undefined)
				properties.height = console.screen_rows;
			else if(isNaN(height) || (y + height - 1) > (console.screen_rows))
				throw("invalid height: " + height);
			else
				properties.height = height;
		});
		
		/* public methods */
		this.cycle = function() {
			var updates = getUpdateList().sort(updateSort);
			var lasty = undefined;
			var lastx = undefined;
			var lastid = undefined;
			for each(var u in updates) {
				var posx = u.x + properties.x;
				var posy = u.y + properties.y;
				if(lasty !== u.y || lastx == undefined || (u.x - lastx) != 1) 
					console.gotoxy(posx,posy);
				if(lastid !== u.id)
					console.attributes = undefined;
				drawChar(u.ch,u.attr,posx,posy);
				lastx = u.x;
				lasty = u.y;
				lastid = u.id;
			}
			properties.update = {};
 		}
		this.draw = function() {
		}
		this.add = function(frame) {
			var canvas = new Canvas(frame,this);
			properties.canvas[frame.id] = canvas;
			this.updateFrame(frame);
		}
		this.remove = function(frame) {
			this.updateFrame(frame);
			delete properties.canvas[frame.id];
		}
		this.top = function(frame) {
			var canvas = properties.canvas[frame.id];
			delete properties.canvas[frame.id];
			properties.canvas[frame.id] = canvas;
			this.updateFrame(frame);
		}
		this.bottom = function(frame) {
			for(var c in properties.canvas) {
				if(c == frame.id)
					continue;
				var canvas = properties.canvas[c];
				delete properties.canvas[c];
				properties.canvas[c] = canvas;
			}
			this.updateFrame(frame);
		}
		this.updateFrame = function(frame) {
			var xoff = frame.x - this.x;
			var yoff = frame.y - this.y;
			for(var y = 0;y<frame.height;y++) {
				for(var x = 0;x<frame.width;x++) {
					updateChar(xoff + x,yoff + y);
				}
			}
		}
		this.updateChar = function(frame,x,y) {
			var xoff = frame.x - this.x;
			var yoff = frame.y - this.y;
			updateChar(xoff + x,yoff + y);
		}
				
		/* private functions */
		function updateChar(x,y) {
			if(!properties.update[y])
				properties.update[y] = {};
			properties.update[y][x] = 1;
		}
		function getUpdateList() {
			var list = [];
			for(var y in properties.update) {
				for(var x in properties.update[y]) {
					var d = getTopChar(x,y);
					list.push({
						ch:d.ch,
						attr:d.attr,
						id:d.id,
						x:Number(x),
						y:Number(y)
					});
				}
			}
			return list;
		}
		function updateSort(a,b) {
			if(a.y == b.y)
				return a.x-b.x;
			return a.y-b.y;
		}
		function drawChar(ch,attr,xpos,ypos) {
			if(attr)
				console.attributes = attr;
			if(xpos == console.screen_columns && ypos == console.screen_rows) 
				console.cleartoeol();
			else if(ch == undefined)
				console.write(" ");
			else 
				console.write(ch);
		}
		function getTopChar(x,y) {
			var top = {};
			for each(var c in properties.canvas) {
				var d = c.getData(x,y);
				if(d) 
					top = d;
			}
			return top;
		}

		/* initialize display properties */
		this.x = x;
		this.y = y;
		this.width = width;
		this.height = height;
		log(LOG_DEBUG,format("new display initialized: %sx%s at %s,%s",this.width,this.height,this.x,this.y));
	}
	
	/* coordinate reference */
	function Coord(x,y) {
		this.x = Number(x);
		this.y = Number(y);
	}
	
	/* character/attribute pair representing a screen position and its contents */
	function Char(ch,attr,id) {
		this.ch = ch;
		this.attr = attr;
		this.id = id;
	}
	
	/* private properties */
	var properties = {
		x:undefined,
		y:undefined,
		width:undefined,
		height:undefined,
		attr:undefined,
		display:undefined,
		data:[],
		id:0
	}
	var relations = {
		parent:undefined,
		child:[]
	}
	var position = {
		cursor:{x:0,y:0},
		offset:{x:0,y:0},
		stored:{x:0,y:0}
	}
		
	/* protected properties */
	this.__defineGetter__("id", function() {
		if(relations.parent)
			return relations.parent.id+""+properties.id;
		return properties.id;
	});
	this.__defineGetter__("parent", function() {
		return relations.parent;
	});
	this.__defineGetter__("child", function() {
		return relations.child;
	});
	this.__defineSetter__("child", function(frame) {
		relations.child.push(frame);
	});
	this.__defineGetter__("display", function() {
		return properties.display;
	});
	this.__defineGetter__("attr", function() {
		return properties.attr;
	});
	this.__defineSetter__("attr", function(attr) {
		properties.attr = attr;
	});
	this.__defineGetter__("name", function() {
		return properties.name;
	});
	this.__defineSetter__("name", function(name) {
		properties.name = name;
	});
	this.__defineGetter__("x", function() { 
		if(properties.x)
			return properties.x;
		return properties.display.x; 
	});
	this.__defineSetter__("x", function(x) {
		if(x == undefined)
			return;
		if(x < 1 || isNaN(x))
			throw("invalid x coordinate: " + x);
		else if(x > (properties.display.x + properties.display.width - 1) || x < properties.display.x)
			throw("invalid x coordinate: " + x);
		properties.x = x;
	});
	this.__defineGetter__("y", function() { 
		if(properties.y)
			return properties.y;
		return properties.display.y; 
	});
	this.__defineSetter__("y", function(y) {
		if(y == undefined)
			return;
		if(y < 1 || isNaN(y))
			throw("invalid y coordinate: " + y);
		else if(y > (properties.display.y + properties.display.height - 1) || y < properties.display.y)
			throw("invalid y coordinate: " + y);
		properties.y = y;
	});
	this.__defineGetter__("width", function() {
		if(properties.width)
			return properties.width;
		return properties.display.width;
	});
	this.__defineSetter__("width", function(width) {
		if(width == undefined)
			return;
		else if(width < 1 || isNaN(width))
			throw("invalid width: " + width);
		else if((properties.x + width) > (properties.display.x + properties.display.width))
			throw("invalid width: " + width);
		properties.width = width;
	});
	this.__defineGetter__("height", function() {
		if(properties.height)
			return properties.height;
		return properties.display.height;
	});
	this.__defineSetter__("height", function(height) {
		if(height == undefined)
			return;
		else if(height < 1 || isNaN(height)) 
			throw("invalid height: " + height);
		else if((properties.y+ height) > (properties.display.y + properties.display.height))
			throw("invalid height: " + height);
		properties.height = height;
	});

	/* public methods */
	this.getData = function(x,y) {
		return properties.data[x + position.offset.x][y + position.offset.y];
	}
	this.setData = function(x,y,ch,attr) {
		if(ch)
			properties.data[x + position.offset.x][y + position.offset.y].ch = ch;
		if(attr)
			properties.data[x + position.offset.x][y + position.offset.y].attr = attr;
	}
	this.bottom = function() {
		for each(var c in relations.child) 
			c.bottom();
		properties.display.bottom(this);
	}
	this.top = function() {
		properties.display.top(this);
		for each(var c in relations.child) 
			c.top();
	}
	this.open = function() {
		properties.display.add(this, properties.data);
		for each(var c in relations.child) 
			c.open();
	}
	this.refresh = function() {
		properties.display.updateFrame(this);
		for each(var c in relations.child) 
			c.refresh();
	}
	this.close = function() {
		for each(var c in relations.child) 
			c.close();
		properties.display.remove(this);
	}
	this.move = function(x,y) {
		if(this.x+x < properties.display.x ||
			this.x+x + this.width > properties.display.x + properties.display.width)
			return false;
		if(this.y+y < properties.display.y ||
			this.y+y + this.height > properties.display.y + properties.display.height)
			return false;
		properties.display.updateFrame(this);
		this.x+=x;
		this.y+=y;
		properties.display.updateFrame(this);
		for each(var c in relations.child) 
			c.move(x,y);
	}
	this.moveTo = function(x,y) {
		if(x < properties.display.x || x + this.width > properties.display.x + properties.display.width)
			return false;
		if(y < properties.display.y || y + this.height > properties.display.y + properties.display.height)
			return false;
		properties.display.updateFrame(this);
		this.x=x;
		this.y=y;
		properties.display.updateFrame(this);
		for each(var c in relations.child) 
			c.moveTo(x + (c.x - this.x), y + (c.y - this.y));
	}
	this.draw = function() {
		this.refresh();
		this.cycle();
	}
	this.cycle = function() {
		return properties.display.cycle();
	}
	this.load = function(filename,width,height) {
		var f=new File(filename);
		switch(file_getext(filename).substr(1).toUpperCase()) {
		case "ANS":
			if(!(f.open("r",true,4096)))
				return(false);
			var lines=f.readAll(4096);
			f.close();
			var attr = this.attr;
			var bg = BG_BLACK;
			var fg = LIGHTGRAY;
			var i = 0;

			var y = 0;
			while(lines.length > 0) {	
				var x = 0;
				var line = lines.shift();
				while(line.length > 0) {
					/* check line status */
					if(x >= this.width) {
						x = 0;
						y++;
					}
					/* parse an attribute sequence*/
					var m = line.match(/^\x1b\[(\d+);?(\d*);?(\d*)m/);
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
					/* parse a positional sequence */
					var n = line.match(/^\x1b\[(\d+)C/);
					if(n !== null) {
						line = line.substr(n.shift().length);
						x+=Number(n.shift());
						continue;
					}
					/* set character and attribute */
					var ch = line[0];
					line = line.substr(1);
					properties.data[x][y]=new Char(ch,attr,this.id);
					x++;
				}
				y++;
			}
			this.open();
			break;
		case "BIN":
			if(width == undefined || height == undefined)
				throw("unknown graphic dimensions");
			if(!(f.open("rb",true,4096)))
				return(false);
			for(var y=0; y<height; y++) {
				for(var x=0; x<width; x++) {
					var c = new Char();
					if(f.eof)
						return(false);
					c.ch = f.read(1);
					if(f.eof)
						return(false);
					c.attr = f.readBin(1);
					c.id = this.id;
					properties.data[x][y] = c;
				}
			}
			f.close();
			this.open();
			break;
		default:
			throw("unsupported filetype");
			break;
		}
	}
	this.scroll = function(x,y) {
		/* default: add a new line to the data matrix */
		if(x == undefined && y == undefined) {
			for(var x = 0;x<this.width;x++) {
				for(var y = 0;y<this.height;y++) 
					properties.display.updateChar(this,x,y);
				properties.data[x].push(new Char(undefined,this.attr,this.id));
			}
			position.offset.y++;
		}
		/* otherwise, adjust the x/y offset */
		else {
			if(typeof x == "number")
				position.offset.x += x;
			if(typeof y == "number")
				position.offset.y += y;
			if(position.offset.x < 0)
				position.offset.x = 0;
			if(position.offset.y < 0)
				position.offset.y = 0;
			if(position.offset.x + this.width > properties.data.length)
				position.offset.x = properties.data.length - this.width;
			if(position.offset.y + this.height > properties.data[0].length)
				position.offset.y = properties.data[0].length - this.height;
		}
	}

	/* console method emulation */
	this.home = function() {
		position.cursor.x = 0;
		position.cursor.y = 0;
	}
	this.clear = function(attr) {
		if(attr == undefined)
			attr = this.attr;
		for(var x = 0;x<this.width;x++) {
			for(var y = 0;y<this.height;y++) {
				properties.data[x][y].ch = undefined;
				properties.data[x][y].attr = attr;
				properties.display.updateChar(this,x,y);
			}
		}
		this.home();
	}
	this.clearline = function(attr) {
		if(attr == undefined)
			attr = this.attr;
		for(var x = 0;x<this.width;x++) {
			properties.display.updateChar(this,x,y);
			properties.data[x][y].ch = undefined;
			properties.data[x][y].attr = attr;
		}
	}
	this.cleartoeol = function(attr) {
		if(attr == undefined)
			attr = this.attr;
		for(var x = position.cursor.x;x<this.width;x++) {
			properties.display.updateChar(this,x,y);
			properties.data[x][y].ch = undefined;
			properties.data[x][y].attr = attr;
		}
	}
	this.crlf = function() {
		position.cursor.x = 0;
		if(position.cursor.y < this.height-1) 
			position.cursor.y += 1;
		else {}
	}
	this.putmsg = function(str) {
		str = str.split('');
		var control_a = false;
		var curattr = this.attr;
		var pos = position.cursor;

		while(str.length > 0) {
			var ch = str.shift();
			if(control_a) {
				var k = ch;
				if(k)
					k = k.toUpperCase();
				switch(k) {
				case '\1':	/* A "real" ^A code */
					putChar.call(this,ch,curattr);
					pos.x++;
					break;
				case 'K':	/* Black */
					curattr=(curattr)&0xf8;
					break;
				case 'R':	/* Red */
					curattr=((curattr)&0xf8)|RED;
					break;
				case 'G':	/* Green */
					curattr=((curattr)&0xf8)|GREEN;
					break;
				case 'Y':	/* Yellow */
					curattr=((curattr)&0xf8)|BROWN;
					break;
				case 'B':	/* Blue */
					curattr=((curattr)&0xf8)|BLUE;
					break;
				case 'M':	/* Magenta */
					curattr=((curattr)&0xf8)|MAGENTA;
					break;
				case 'C':	/* Cyan */
					curattr=((curattr)&0xf8)|CYAN;
					break;
				case 'W':	/* White */
					curattr=((curattr)&0xf8)|LIGHTGRAY;
					break;
				case '0':	/* Black */
					curattr=(curattr)&0x8f;
					break;
				case '1':	/* Red */
					curattr=((curattr)&0x8f)|(RED<<4);
					break;
				case '2':	/* Green */
					curattr=((curattr)&0x8f)|(GREEN<<4);
					break;
				case '3':	/* Yellow */
					curattr=((curattr)&0x8f)|(BROWN<<4);
					break;
				case '4':	/* Blue */
					curattr=((curattr)&0x8f)|(BLUE<<4);
					break;
				case '5':	/* Magenta */
					curattr=((curattr)&0x8f)|(MAGENTA<<4);
					break;
				case '6':	/* Cyan */
					curattr=((curattr)&0x8f)|(CYAN<<4);
					break;
				case '7':	/* White */
					curattr=((curattr)&0x8f)|(LIGHTGRAY<<4);
					break;
				case 'H':	/* High Intensity */
					curattr|=HIGH;
					break;
				case 'I':	/* Blink */
					curattr|=BLINK;
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
					pos.x=0;
					break;
				case ']':	/* LF */
					pos.y++;
					break;
				default:	/* Other stuff... specifically, check for right movement */
					if(ch.charCodeAt(0)>127) {
						pos.x+=ch.charCodeAt(0)-127;
						if(pos.x>=this.width)
							pos.x=this.width-1;
					}
					break;
				}
				control_a = false;
			}
			else {
				switch(ch) {
				case '\1':		/* CTRL-A code */
					control_a = true;
					break;
				case '\7':		/* Beep */
					break;
				case '\r':
					pos.x=0;
					break;
				case '\n':
					pos.y++;
					break;
				default:
					putChar.call(this,ch,curattr);
					pos.x++;
					break;
				}
			}
		}
	}
	this.center = function(str) {
		position.cursor.x = Math.ceil(this.width/2) - Math.ceil(console.strlen(strip_ctrl(str))/2);
		if(position.cursor.x < 0)
			position.cursor.x = 0;
		this.putmsg(str);
	}
	this.gotoxy = function(x,y) {
		if(x <= this.width)
			position.cursor.x = x-1;
		if(y <= this.height)
			position.cursor.y = y-1;
	}
	this.getxy = function() {
		var xy = {
			x:position.cursor.x+1,
			y:position.cursor.y+1
		}
		return xy;
	}
	this.pushxy = function() {
		position.stored.x = position.cursor.x;
		position.stored.y = position.cursor.y;
	}
	this.popxy = function() {
		position.cursor.x = position.stored.x;
		position.cursor.y = position.stored.y;
	}
	
	/* private functions */
	function putChar(ch,attr) {
		if(position.cursor.x >= this.width) {
			position.cursor.x=0;
			position.cursor.y++;
		}
		if(position.cursor.y >= this.height) {	
			this.scroll();
			position.cursor.y--;
		}
		properties.data
			[position.cursor.x + position.offset.x]
			[position.cursor.y + position.offset.y].ch = ch;
		properties.data
			[position.cursor.x + position.offset.x]
			[position.cursor.y + position.offset.y].attr = attr;
		properties.display.updateChar(this,position.cursor.x,position.cursor.y);
	}
	function init(x,y,width,height,attr,frame) {
		if(frame instanceof Frame) {
			properties.id = frame.child.length;
			properties.display = frame.display;
			relations.parent = frame;
			frame.child = this;
		}
		else {
			properties.display = new Display(x,y,width,height);
		}

		this.transparent = false;
		this.x = x;
		this.y = y;
		this.width = width;
		this.height = height;
		this.attr = attr;
		
		for(var w=0;w<this.width;w++) {
			properties.data.push(new Array(this.height));
			for(var h=0;h<this.height;h++) {
				properties.data[w][h] = new Char(undefined,this.attr,this.id);
			}
		}
		//log(LOG_DEBUG,format("new frame initialized: %sx%s at %s,%s",this.width,this.height,this.x,this.y));
	}
	init.apply(this,arguments);
}

if(argc >= 4)
	frame = new Frame(argv[0],argv[1],argv[2],argv[3],argv[4],argv[5]);
