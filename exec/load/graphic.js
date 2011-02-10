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

	this.scrollback=500;
	this.loop=false;
	this.atcodes=true;
	this.length=0;
	this.index=0;
	
	this.data=new Array(w);
	for(var y=0; y<this.height; y++) {
		for(var x=0; x<this.width; x++) {
			if(y==0) {
				this.data[x]=new Array(h);
			}
			this.data[x][y]=new Graphic_sector(this.ch,this.attribute);
		}
	}
	
	this.gety=Graphic_gety;
	this.home=Graphic_home;
	this.end=Graphic_end;
	this.pgup=Graphic_pgup;
	this.pgdn=Graphic_pgdn;
	this.draw=Graphic_draw;
	this.drawfx=Graphic_drawfx;
	this.drawslow=Graphic_drawslow;
	this.load=Graphic_load;
	this.write=Graphic_write;
	this.scroll=Graphic_scroll;
	this.putmsg=Graphic_putmsg;
	this.resize=Graphic_resize;
}
function Graphic_sector(ch,attr)
{
	this.ch=ch;
	this.attr=attr;
}
function Graphic_gety()
{
	var y=this.length;
	if(y>=this.height) {
		y=this.height;
	}
	return y;
}
function Graphic_draw(xpos,ypos,width,height,xoff,yoff,delay)
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
	if(delay==undefined)
		delay=0;
	if(xoff+width > this.width || yoff+height > this.height) {
		alert("Attempt to draw from outside of graphic: "+xoff+":"+yoff+" "+width+"x"+height+" "+this.width+"x"+this.height);
		return(false)
	}
	if(xpos+width-1 > console.screen_columns || ypos+height-1 > console.screen_rows) {
		alert("Attempt to draw outside of screen: " + (xpos+width-1) + "x" + (ypos+height-1));
		return(false);
	}
	for(y=0;y<height; y++) {
		console.gotoxy(xpos,ypos+y);
		for(x=0; x<width; x++) {
			// Do not draw to the bottom left corner of the screen-would scroll
			if(xpos+x != console.screen_columns
					|| ypos+y != console.screen_rows) {
				console.attributes=this.data[x+xoff][this.index+y+yoff].attr;
				var ch=this.data[x+xoff][this.index+y+yoff].ch;
				if(ch == "\r" || ch == "\n" || !ch)
					ch=this.ch;
				console.write(ch);
			}
		}
		if(delay)
			mswait(delay);
	}
	return(true);
}
function Graphic_drawslow(xpos,ypos,width,height,xoff,yoff)
{
	this.draw(xpos,ypos,width,height,xoff,yoff,2);
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
			placeholder[x][y]={'x':xoff+x,'y':this.index+yoff+y};
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
			var ch=this.data[position.x][position.y].ch;
			if(ch == "\r" || ch == "\n" || !ch)
				ch=this.ch;
			console.write(ch);
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
	var file_type=file_getext(filename).substr(1);
	var f=new File(filename);

	switch(file_type.toUpperCase()) {
	case "ANS":
	/*
		if(!(f.open("r",true,4096)))
			return(false);
		var lines=f.readAll();
		f.close();
		
		var cur_attr=this.attr;
		var cur_ch=this.ch;
		var ANSI=String.fromCharCode(27,91);
		
		for(y=0; y<this.height && y<lines.length; y++) {
			var x=0;
			var index=0;
			index=lines[y].indexOf(ANSI,index);
			while(index >= 0) {
			
			}
			
			txt=txt.toString().replace(/@(.*)@/g,
				function (str, code, offset, s) {
					return bbs.atcode(code);
				}
			)
			this.data[x][y]=new Graphic_sector(cur_ch,cur_attr);
		}
		break;
	*/
	case "BIN":
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
		f.close();
		break;
	}
	return(true);
}
function Graphic_write(filename)
{
	var x;
	var y;
	var f=new File(filename);

	if(!(f.open("wb",true,4096)))
		return(false);
	for(y=0; y<this.height; y++) {
		for(x=0; x<this.width; x++) {
			f.write(this.data[x][y].ch);
			f.writeBin(this.data[x][y].attr,1);
		}
	}
	f.close();
	return(true);
}
function Graphic_end()
{
	var newindex=this.data[0].length-this.height;
	if(newindex == this.index) return false;
	this.index=newindex;
	return true;
}
function Graphic_pgup()
{
	this.index -= this.height;
	if(this.index < 0) this.index = 0;
}
function Graphic_pgdn()
{
	this.index += this.height;
	if(this.index + this.height >= this.data[0].length) {	
		this.index=this.data[0].length-this.height;
	}
}
function Graphic_home()
{
	if(this.index == 0) return false;
	this.index=0;
	return true;
}
function Graphic_scroll(dir)
{
	switch(dir){
	case 1:
		if(this.index + this.height >= this.data[0].length) {
			if(!this.loop) return false;
		}
		this.index++;
		break;
	case -1:
		if(this.index == 0) {
			if(!this.loop) return false;
			this.index=this.height-1;
		}
		this.index--;
		break;
	default:
		for(var x=0; x<this.width; x++) {
			this.data[x].push(new Graphic_sector(this.ch,this.attribute));
			if(this.data[x].length > this.scrollback) {
				this.data.shift();
			}
		}
		this.index=this.data[0].length-this.height;
		if(this.length < this.scrollback) this.length++;
		break;
	}
	return true;
}
function Graphic_resize(w,h)
{
	this.data=new Array(w);
	if(w) this.width=w;
	if(h) this.height=h;
	this.index=0;
	this.length=0;
	for(var y=0; y<this.height; y++) {
		for(var x=0; x<this.width; x++) {
			if(y==0) {
				this.data[x]=new Array(this.height);
			}
			this.data[x][y]=new Graphic_sector(this.ch,this.attribute);
		}
	}
}
/* Returns the number of times scrolled */
function Graphic_putmsg(xpos, ypos, txt, attr, scroll)
{
	var curattr=attr;
	var ch;
	var x=xpos?xpos-1:0;
	var y=ypos?ypos-1:this.gety();
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
		)
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
			case '\1':		/* CTRL-A code */
				ch=txt[p++].toUpperCase();
				switch(ch) {
					case '\1':	/* A "real" ^A code */
						this.data[x][this.index + y].ch=ch;
						this.data[x][this.index + y].attr=curattr;
						x++;
						if(x>=this.width) {
							x=0;
							y++;
							log("next char: [" + txt[p] + "]");
							if(txt[p] == '\r') p++;
							if(txt[p] == '\n') p++;
							this.length++;
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
						this.length++;
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
				this.length++;
				y++;
				break;
			default:
				this.data[x][this.index + y]=new Graphic_sector(ch,curattr);
				x++;
				if(x>=this.width) {
					x=0;
					y++;
					if(txt[p] == '\r') p++;
					if(txt[p] == '\n') p++;
					this.length++;
				}
		}
	}
	return(scrolls);
}

