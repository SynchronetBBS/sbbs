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

 		x: 		the coordinate representing the top left corner of the frame (horiz)
 		y: 		the coordinate representing the top left corner of the frame (vert)
 		width: 	the horizontal width of the frame 
 		height: 	the vertical height of the frame
 		attr:		the background color of the frame (displayed when there are no other contents)
		
METHODS:

	frame.open();				//populates frame contents in character map
	frame.close();				//removes frame contents from character map
	frame.draw();				//draws the characters occupied by 'frame' coords/dimensions 
	frame.cycle();				//checks the display matrix for updated characters and displays them 
	frame.load(filename):			//loads a binary graphic (.BIN) or ANSI graphic (.ANS) file into the frame
	frame.bottom();				//push frame to bottom of display stack
	frame.top();					//pull frame to top of display stack
	frame.scroll(dir);				//scroll frame one line in either direction ***NOT YET IMPLEMENTED***
	frame.move(x,y);				//move frame one space where x = -1,0,1 and y = -1,0,1 
	frame.moveTo(x,y);				//move frame to absolute position
	
	frame.clearline(attr);			//see http://synchro.net/docs/jsobjs.html#console
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
	
	.//beware this sample infinite loop
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

	/* matrix representing frame positional and dimensional limits */
	function CharMap(x,y,width,height) {
		/* private properties */
		var properties = {
			x:undefined,
			y:undefined,
			width:undefined,
			height:undefined,
			map:[],
			update:{}
		}

		/* protected properties */
		this.x getter = function() {
			return properties.x;
		}
		this.x setter = function(x) {
			if(x == undefined)
				properties.x = 1;
			else if(isNaN(x) || x < 1 || x > console.screen_columns)
				throw("invalid x coordinate: " + x);
			else 
				properties.x = x;
		}
		this.y getter = function() {
			return properties.y;
		}
		this.y setter = function(y) {
			if(y == undefined)
				properties.y = 1;
			else if(isNaN(y) || y < 1 || y > console.screen_rows)
				throw("invalid y coordinate: " + y);
			else 
				properties.y = y;
		}
		this.width getter = function() {
			return properties.width;
		}
		this.width setter = function(width) {
			if(width == undefined)
				properties.width = console.screen_columns;
			else if(isNaN(width) || (x + width - 1) > (console.screen_columns))
				throw("invalid width: " + width);
			else 
				properties.width = width;
		}
		this.height getter = function() {
			return properties.height;
		}
		this.height setter = function(height) {
			if(height == undefined)
				properties.height = console.screen_rows;
			else if(isNaN(height) || (y + height - 1) > (console.screen_rows))
				throw("invalid height: " + height);
			else
				properties.height = height;
		}
		
		/* public methods */
		this.cycle = function() {
			for(var y in properties.update) {
				var lastx = undefined;
				for(var x in properties.update[y]) {
					var posx = Number(x) + properties.x;
					var posy = Number(y) + properties.y;
					if(lastx == undefined || x - lastx !== 1) 
						console.gotoxy(posx,posy);
					drawChar(x,y,posx,posy);
					lastx = Number(x);
				}
			}
			properties.update = {};
 		}
		this.draw = function(xpos,ypos,width,height) {
			var xoff = xpos - properties.x;
			var yoff = ypos - properties.y;
			for(var y=0;y<height;y++) {
				console.gotoxy(xpos,ypos + y);
				for(var x=0;x<width;x++) {
					drawChar(x+xoff,y+yoff,xpos,ypos);
				}
			}
		}
		this.add = function(frame) {
			for(var xoff = 0;xoff<frame.width;xoff++) {
				for(var yoff = 0;yoff<frame.height;yoff++) {
					var x = frame.x - properties.x;
					var y = frame.y - properties.y;
					properties.map[xoff+x][yoff+y][frame.id] = 
						new CharPointer(frame,xoff,yoff);
					updateChar(xoff + x,yoff + y);
				}
			}
		}
		this.remove = function(frame) {
			for(var xoff = 0;xoff<frame.width;xoff++) {
				for(var yoff = 0;yoff<frame.height;yoff++) {
					var x = frame.x - properties.x;
					var y = frame.y - properties.y;
					delete properties.map[xoff+x][yoff+y][frame.id];
					updateChar(xoff + x,yoff + y);
				}
			}
		}
		this.top = function(frame) {
			for(var xoff = 0;xoff<frame.width;xoff++) {
				for(var yoff = 0;yoff<frame.height;yoff++) {
					var x = frame.x - properties.x;
					var y = frame.y - properties.y;
					delete properties.map[xoff+x][yoff+y][frame.id];
					properties.map[xoff+x][yoff+y][frame.id] = 
						new CharPointer(frame,xoff,yoff);
					updateChar(xoff + x,yoff + y);
				}
			}
		}
		this.bottom = function(frame) {
			for(var xoff = 0;xoff<frame.width;xoff++) {
				for(var yoff = 0;yoff<frame.height;yoff++) {
					var x = frame.x - properties.x;
					var y = frame.y - properties.y;
					for(var f in properties.map[xoff+x][yoff+y]) {
						var cp = properties.map[xoff+x][yoff+y][f];
						if(cp.frame.id == frame.id)
							continue;
						delete properties.map[xoff+x][yoff+y][f];
						properties.map[xoff+x][yoff+y][f] = 
							cp;
					}
					updateChar(xoff + x,yoff + y);
				}
			}
		}
		this.update = function(frame,xoff,yoff) {
			var x = frame.x - properties.x;
			var y = frame.y - properties.y;
			updateChar(xoff + x,yoff + y);
		}
		
		/* private functions */
		function updateChar(x,y) {
			if(!properties.update[y])
				properties.update[y] = {};
			properties.update[y][x] = 1;
		}
		function drawChar(x,y,xpos,ypos) {
			var sector = getLast(properties.map[x][y]);
			var ch = undefined;
			var attr = undefined;

			if(sector instanceof CharPointer) {
				ch = sector.frame.data[sector.x][sector.y].ch;
				attr = sector.frame.data[sector.x][sector.y].attr;
			}
			console.attributes = attr;
			if(xpos == console.screen_columns && ypos == console.screen_rows) 
				console.cleartoeol();
			else if(ch == undefined)
				console.write(" ");
			else 
				console.write(ch);
		}
		function init(x,y,width,height) {
			this.x = x;
			this.y = y;
			this.width = width;
			this.height = height;
			
			for(var w = 0;w<this.width;w++) {
				properties.map.push(new Array(height));
				for(var h = 0;h<this.height;h++)
					properties.map[w][h] = {}; 
			}
		}
		init.apply(this,arguments);
	}
	
	/* frame and coordinate reference */
	function CharPointer(frame,x,y) {
		this.frame = frame;
		this.x = x;
		this.y = y;
	}
	
	/* character/attribute pair representing a screen position and its contents */
	function Char(ch,attr) {
		this.ch = ch;
		this.attr = attr;
	}
	
	/* private properties */
	var properties = {
		x:undefined,
		y:undefined,
		width:undefined,
		height:undefined,
		attr:undefined,
		map:undefined,
		data:[],
		parent:undefined,
		child:[],
		cursor:{x:0,y:0},
		id:0
	}
		
	/* protected properties */
	this.id getter = function() {
		if(properties.parent)
			return properties.parent.id+""+properties.id;
		return properties.id;
	}
	this.parent getter = function() {
		return properties.parent;
	}
	this.child getter = function() {
		return properties.child;
	}
	this.child setter = function(frame) {
		properties.child.push(frame);
	}
	this.data getter = function() {
		return properties.data;
	}
	this.map getter = function() {
		return properties.map;
	}
	this.attr getter = function() {
		return properties.attr;
	}
	this.attr setter = function(attr) {
		properties.attr = attr;
	}
	this.name getter = function() {
		return properties.name;
	}
	this.name setter = function(name) {
		properties.name = name;
	}
	this.x getter = function() { 
		if(properties.x)
			return properties.x;
		return properties.map.x; 
	}
	this.x setter = function(x) {
		if(x == undefined)
			return;
		if(x < 1 || isNaN(x))
			throw("invalid x coordinate: " + x);
		else if(x > (properties.map.x + properties.map.width - 1) || x < properties.map.x)
			throw("invalid x coordinate: " + x);
		properties.x = x;
	}
	this.y getter = function() { 
		if(properties.y)
			return properties.y;
		return properties.map.y; 
	}
	this.y setter = function(y) {
		if(y == undefined)
			return;
		if(y < 1 || isNaN(y))
			throw("invalid y coordinate: " + y);
		else if(y > (properties.map.y + properties.map.height - 1) || y < properties.map.y)
			throw("invalid y coordinate: " + y);
		properties.y = y;
	}
	this.width getter = function() {
		if(properties.width)
			return properties.width;
		return properties.map.width;
	}
	this.width setter = function(width) {
		if(width == undefined)
			return;
		else if(width < 1 || isNaN(width))
			throw("invalid width: " + width);
		else if((properties.x + width) > (properties.map.x + properties.map.width))
			throw("invalid width: " + width);
		properties.width = width;
	}
	this.height getter = function() {
		if(properties.height)
			return properties.height;
		return properties.map.height;
	}
	this.height setter = function(height) {
		if(height == undefined)
			return;
		else if(height < 1 || isNaN(height)) 
			throw("invalid height: " + height);
		else if((properties.y+ height) > (properties.map.y + properties.map.height))
			throw("invalid height: " + height);
		properties.height = height;
	}

	/* public methods */
	this.bottom = function() {
		properties.map.bottom(this);
	}
	this.top = function() {
		properties.map.top(this);
	}
	this.open = function() {
		properties.map.add(this);
		for each(var c in properties.child) 
			c.open();
	}
	this.close = function() {
		for each(var c in properties.child) 
			c.close();
		properties.map.remove(this);
	}
	this.move = function(x,y) {
		if(this.x+x < properties.map.x ||
			this.x+x + this.width > properties.map.x + properties.map.width)
			return false;
		if(this.y+y < properties.map.y ||
			this.y+y + this.height > properties.map.y + properties.map.height)
			return false;
		properties.map.remove(this);
		this.x += x;
		this.y += y;
		properties.map.add(this);
	}
	this.moveTo = function(x,y) {
		if(x < properties.map.x || x + this.width > properties.map.x + properties.map.width)
			return false;
		if(y < properties.map.y || y + this.height > properties.map.y + properties.map.height)
			return false;
		properties.map.remove(this);
		this.x = x;
		this.y = y;
		properties.map.add(this);
	}
	this.draw = function() {
		properties.map.draw(this.x,this.y,this.width,this.height);
	}
	this.cycle = function() {
		return properties.map.cycle();
	}
	this.load = function(filename) {
		var f=new File(filename);
		switch(file_getext(filename).substr(1).toUpperCase()) {
		case "ANS":
			if(!(f.open("r",true,4096)))
				return(false);
			var lines=f.readAll();
			f.close();
			
			var attr = this.attr;
			var bg = BG_BLACK;
			var fg = LIGHTGRAY;
			var i = 0;

			var y = 0;
			while(lines.length > 0 && y < this.height) {	
				var x = 0;
				var line = lines.shift();
				while(line.length > 0 && x < this.width) {
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
					properties.data[x][y]=new Char(ch,attr);
					x++;
				}
				y++;
			}
			break;
		case "BIN":
			if(!(f.open("rb",true,4096)))
				return(false);
			for(var y=0; y<this.height; y++) {
				for(var x=0; x<this.width; x++) {
					var c = new Char();
					if(f.eof)
						return(false);
					c.ch = f.read(1);
					if(f.eof)
						return(false);
					c.attr = f.readBin(1);
					properties.data[x][y] = c;
				}
			}
			f.close();
			break;
		default:
			throw("unsupported filetype");
			break;
		}
	}
	this.scroll = function(dir) {
		if(dir == undefined) {
			for(var x = 0;x<this.width;x++) {
				for(var y = 0;y<this.height;y++) {
					if(properties.data[x][y+1]) {
						properties.data[x][y].ch = properties.data[x][y+1].ch;
						properties.data[x][y].attr = properties.data[x][y+1].attr;
					}
					else {
						properties.data[x][y].ch = undefined;
						properties.data[x][y].attr = this.attr;
					}
					properties.map.update(this,x,y);
				}
			}
		}
	}

	/* console method emulation */
	this.home = function() {
		properties.cursor.x = 0;
		properties.cursor.y = 0;
	}
	this.clear = function(attr) {
		if(attr == undefined)
			attr = this.attr;
		for(var x = 0;x<this.width;x++) {
			for(var y = 0;y<this.height;y++) {
				properties.data[x][y].ch = undefined;
				properties.data[x][y].attr = attr;
				properties.map.update(this,x,y);
			}
		}
		this.home();
	}
	this.clearline = function(attr) {
		if(attr == undefined)
			attr = this.attr;
		for(var x = 0;x<this.width;x++) {
			properties.map.update(this,x,y);
			properties.data[x][y].ch = undefined;
			properties.data[x][y].attr = attr;
		}
	}
	this.cleartoeol = function(attr) {
		if(attr == undefined)
			attr = this.attr;
		for(var x = properties.cursor.x;x<this.width;x++) {
			properties.map.update(this,x,y);
			properties.data[x][y].ch = undefined;
			properties.data[x][y].attr = attr;
		}
	}
	this.crlf = function() {
		properties.cursor.x = 0;
		if(properties.cursor.y < this.height-1) 
			properties.cursor.y += 1;
		else {}
	}
	this.putmsg = function(str) {
		str = str.split('');
		var control_a = false;
		var curattr = this.attr;
		var pos = properties.cursor;

		while(str.length > 0) {
			if(pos.x >= this.width) {
				pos.x=0;
				pos.y++;
			}
			
			if(pos.y >= this.height) {	
				this.scroll();
				pos.y--;
			}
			var ch = str.shift();
			if(control_a) {
				switch(ch) {
				case '\1':	/* A "real" ^A code */
					properties.data[pos.x][pos.y].ch=ch;
					properties.data[pos.x][pos.y].attr=curattr;
					properties.map.update(this,pos.x,pos.y);
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
					properties.data[pos.x][pos.y].ch=ch;
					properties.data[pos.x][pos.y].attr=curattr;
					properties.map.update(this,pos.x,pos.y);
					pos.x++;
					break;
				}
			}
		}
	}
	this.center = function(str) {
		properties.cursor.x = Math.ceil(this.width/2) - Math.ceil(console.strlen(str)/2) + 1;
		if(properties.cursor.x < 0)
			properties.cursor.x = 0;
		this.putmsg(str);
	}
	this.gotoxy = function(x,y) {
		if(x <= this.width)
			properties.cursor.x = x-1;
		if(y <= this.height)
			properties.cursor.y = y-1;
	}
	this.getxy = function() {
		var xy = {
			x:properties.cursor.x+1,
			y:properties.cursor.y+1
		}
		return xy;
	}
	
	/* private functions */
	function getLast(obj) {
		var last = undefined;
		for each(var i in obj)
			last = i;
		return last;
	}
	function init(x,y,width,height,attr,frame) {
		if(frame instanceof Frame) {
			properties.id = frame.child.length;
			properties.map = frame.map;
			properties.parent = frame;
			frame.child = this;
		}
		else {
			properties.map = new CharMap(x,y,width,height);
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
				properties.data[w][h] = new Char(undefined,this.attr);
			}
		}
	}
	init.apply(this,arguments);
}

if(argc >= 4)
	frame = new Frame(argv[0],argv[1],argv[2],argv[3],argv[4],argv[5]);