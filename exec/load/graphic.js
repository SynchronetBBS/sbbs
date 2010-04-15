// $Id$

/*
 * "Graphic" object
 * Allows a graphic to be stored in memory and portions of it redrawn on command
 */

function Graphic(w,h,attr,ch)
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

	this.data=new Array(w);
	this.past=new Array(w);
	this.future=new Array(w);
	this.lines=0;
	var x;
	var y;
	
	for(y=0; y<this.height; y++) {
		for(x=0; x<this.width; x++) {
			if(y==0) {
				this.data[x]=new Array(h);
				this.past[x]=[];
				this.future[x]=[];
			}
			this.data[x][y]=new Graphic_sector(this.ch,this.attribute);
		}
	}
	this.home=Graphic_home;
	this.end=Graphic_end;
	this.pgup=Graphic_pgup;
	this.pgdn=Graphic_pgdn;
	this.draw=Graphic_draw;
	this.drawfx=Graphic_drawfx;
	this.drawslow=Graphic_drawslow;
	this.save=Graphic_save;
	this.load=Graphic_load;
	this.write=Graphic_write;
	this.scroll=Graphic_scroll;
	this.putmsg=Graphic_putmsg;
}
function Graphic_sector(ch,attr)
{
	this.ch=ch;
	this.attr=attr;
}

function Graphic_draw(xpos,ypos,width,height,xoff,yoff)
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
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false)
	}
	if(xpos+width-1 > console.screen_columns || ypos+height-1 > console.screen_rows) {
		alert("Attempt to draw outside of screen");
		return(false);
	}
	for(y=0;y<height; y++) {
		console.gotoxy(xpos,ypos+y);
		for(x=0; x<width; x++) {
			// Do not draw to the bottom left corner of the screen-would scroll
			if(xpos+x != console.screen_columns
					|| ypos+y != console.screen_rows) {
				console.attributes=this.data[x+xoff][y+yoff].attr;
				var ch=this.data[x+xoff][y+yoff].ch;
				console.write(ch?ch:this.ch);
			}
		}
	}
	return(true);
}
function Graphic_drawslow(xpos,ypos,width,height,xoff,yoff)
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
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false)
	}
	if(xpos+width-1 > console.screen_columns || ypos+height-1 > console.screen_rows) {
		alert("Attempt to draw outside of screen");
		return(false);
	}
	for(y=0;y<height; y++) {
		console.gotoxy(xpos,ypos+y);
		for(x=0; x<width; x++) {
			// Do not draw to the bottom left corner of the screen-would scroll
			if(xpos+x != console.screen_columns
					|| ypos+y != console.screen_rows) {
				console.attributes=this.data[x+xoff][y+yoff].attr;
				console.write(this.data[x+xoff][y+yoff].ch);
			}
			mswait(2);
		}
	}
	return(true);
}
function Graphic_drawfx(xpos,ypos,width,height,xoff,yoff)
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
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false)
	}
	if(xpos+width-1 > console.screen_columns || ypos+height-1 > console.screen_rows) {
		alert("Attempt to draw outside of screen");
		return(false);
	}
	var placeholder=new Array(width);
	for(x=0;x<width;x++)
	{
		placeholder[x]=new Array(height);
		for(y=0;y<placeholder[x].length;y++)
		{
			placeholder[x][y]={'x':xoff+x,'y':yoff+y};
		}
	}
	while(placeholder.length)
	{
		var randx=random(placeholder.length);
		var randy=random(placeholder[randx].length);
		var position=placeholder[randx][randy];
		if(position.x != console.screen_columns
				|| position.y != console.screen_rows) {
			console.gotoxy(xpos+position.x,ypos+position.y);
			console.attributes=this.data[position.x][position.y].attr;
			console.write(this.data[position.x][position.y].ch);
			mswait(1);
		}
		placeholder[randx].splice(randy,1);
		if(!placeholder[randx].length) placeholder.splice(randx,1);
	}
	console.gotoxy(xpos+width,ypos+height);
	return(true);
}
function Graphic_load(filename)
{
	var x;
	var y;
	var f=new File(filename);

	if(!(f.open("rb",true,4096)))
		return(false);
	for(y=0; y<this.height; y++) {
		for(x=0; x<this.width; x++) {
			this.data[x][y]=new Object;
			if(f.eof)
				return(false);
			this.data[x][y].ch=f.read(1);
			if(f.eof)
				return(false);
			this.data[x][y].attr=f.readBin(1);
		}
	}
	return(true);
}
function Graphic_write(xpos, ypos, txt, attr)
{
	var x=xpos-1;
	var y=ypos-1;
	var p=0;

	while(p<txt.length && x<this.width && y<this.height) {
		this.data[x][y].ch=txt.substr(p++,1);
		if(attr!=undefined)
			this.data[x][y].attr=attr;
		x++;
		if(x>=this.width) {
			x=0;
			y++;
		}
	}
}
function Graphic_end()
{
	while(this.future[0].length) {
		for(x=0; x<this.width; x++) {
			this.past[x].push(this.data[x].shift());
			this.data[x].push(this.future[x].shift());
		}
	}
}
function Graphic_pgup()
{
	var line=0;
	while(this.past[0].length && line<this.height) {
		for(x=0; x<this.width; x++) {
			this.future[x].unshift(this.data[x].pop());
			this.data[x].unshift(this.past[x].pop());
		}
		line++;
	}
}
function Graphic_pgdn()
{
	var line=0;
	while(this.future[0].length && line<this.height) {
		for(x=0; x<this.width; x++) {
			this.past[x].push(this.data[x].shift());
			this.data[x].push(this.future[x].shift());
		}
		line++;
	}
}
function Graphic_home()
{
	while(this.past[0].length) {
		for(x=0; x<this.width; x++) {
			this.future[x].unshift(this.data[x].pop());
			this.data[x].unshift(this.past[x].pop());
		}
	}
}
function Graphic_scroll(dir)
{
	switch(dir) 
	{
		case 1:
			if(this.future[0].length>0) {
				for(x=0; x<this.width; x++) {
						this.past[x].push(this.data[x].shift());
						this.data[x].push(this.future[x].shift());
				}
			}
			break;
		case -1:
			if(this.past[0].length>0) {
				for(x=0; x<this.width; x++) {
					this.future[x].unshift(this.data[x].pop());
					this.data[x].unshift(this.past[x].pop());
				}
			}
			break;
		default:
			for(x=0; x<this.width; x++) {
					this.past[x].push(this.data[x].shift());
					this.data[x].push(new Graphic_sector(this.ch,this.attribute));
			}
			if(this.lines<this.height) this.lines++;
			break;
	}
}
/* Converts a text string to binary format and appends it to a file */
function Graphic_save(file,txt)
{
	var binFile=new File(file);
	binFile.open('ab');
	
	// if regular text or numbers are passed, append the data to file
	if(typeof txt=='string' || typeof txt=='number') {
		txt=txt.replace(/(\S)/g,"$1");
		binFile.write(txt.replace(/ /g," "));
	}
	// if an array is passed, check to see if it is 2 dimensional, and write data
	else if(typeof txt=='object') { 
		for(t in txt) {
			if(typeof txt[t]=='object') {
				for(tt in text[t]) {
					txt[t][tt]=txt[t][tt].replace(/(\S)/g,"$1");
					binFile.write(txt[t][tt].replace(/ /g," "));
				}
			}
			else {
			txt[t]=txt[t].replace(/(\S)/g,"$1");
			binFile.write(txt[t].replace(/ /g," "));
			}
		}
	}
	else alert("Incompatible data type");
	binFile.close();
}
/* Returns the number of times scrolled */
function Graphic_putmsg(xpos, ypos, txt, attr, scroll)
{
	var curattr=attr;
	var ch;
	var x=xpos?xpos-1:0;
	var y=ypos?ypos-1:this.lines;
	var p=0;
	var scrolls=0;

	if(curattr==undefined)
		curattr=this.attribute;
	/* Expand @-codes */
	if(txt==undefined || txt==null || txt.length==0)
		return(0);
	txt=txt.toString().replace(/@(.*)@/g,
		function (str, code, offset, s) {
			return bbs.atcode(code);
		}
	);
	if(scroll && y==this.height) {
		scrolls++;
		this.scroll();
		y--;
	};
	/* ToDo: Expand \1D, \1T, \1<, \1Z */
	/* ToDo: "Expand" (ie: remove from string when appropriate) per-level/per-flag stuff */
	/* ToDo: Strip ANSI (I betcha @-codes can slap it in there) */
	debug("placing text: " + txt);
	while(p<txt.length && x<this.width && y<this.height) {
		ch=txt.substr(p++,1);
		switch(ch) {
			case '\1':		/* CTRL-A code */
				ch=txt.substr(p++,1).toUpperCase();
				switch(ch) {
					case '\1':	/* A "real" ^A code */
						this.data[x][y].ch=ch;
						this.data[x][y].attr=curattr;
						x++;
						if(x>=this.width) {
							x=0;
							y++;
							if(scroll && y>=this.height) {
								scrolls++;
								this.scroll();
								y--;
							}
						}
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
						x=0;
						break;
					case ']':	/* LF */
						y++;
						if(scroll && y>=this.height) {
							scrolls++;
							this.scroll();
							y--;
						}
						break;
					default:	/* Other stuff... specifically, check for right movement */
						if(ch.charCodeAt(0)>127) {
							x+=ch.charCodeAt(0)-127;
							if(x>=this.width)
								x=this.width-1;
						}
				}
				break;
			case '\7':		/* Beep */
				break;
			case '\r':
				x=0;
				break;
			case '\n':
				if(this.lines<this.height) this.lines++;
				if(p<txt.length-1) {
					y++;
					if(scroll && y>=this.height) {
						scrolls++;
						this.scroll();
						y--;
					}
				}
				break;
			default:
				this.data[x][y].ch=ch;
				this.data[x][y].attr=curattr;
				x++;
				if(x>=this.width) {
					x=0;
					y++;
					if(scroll && y>=this.height) {
						scrolls++;
						this.scroll();
						y--;
					}
				}
		}
	}
	return(scrolls);
}
