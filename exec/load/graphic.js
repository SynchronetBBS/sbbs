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
	var x;
	var y;
	for(y=0; y<this.height; y++) {
		for(x=0; x<this.width; x++) {
			if(y==0) {
				this.data[x]=new Array(h);
			}
			this.data[x][y]=new Object;
			this.data[x][y].ch=ch;
			this.data[x][y].attr=attr;
		}
	}
	this.draw=Graphic_draw;
	this.load=Graphic_load;
	this.write=Graphic_write;
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
		alert("Attempt to draw from outside of graphic");
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
		}
	}
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

function Graphic_scroll(lines)
{
	var x;
	var y;

	if(lines<1)	/* Do not (yet... ToDo) allow negative scroll */
		return;

	for(y=lines; y<this.height; y++) {
		for(x=0; x<this.width; x++) {
			this.data[x][y-lines].ch=this.data[x][y].ch;
			this.data[x][y-lines].attr=this.data[x][y].attr;
		}
	}
	for(y=height-lines; y<this.height; y++) {
		for(x=0; x<this.width; x++) {
			this.data[x][y-lines].ch=this.ch;
			this.data[x][y-lines].attr=this.attribute;
		}
	}
}

function Graphic_putmsg(xpos, ypos, txt, attr, scroll)
{
	var curattr=attr;
	var ch;

	if(curattr==undefined)
		curattr=this.attribute;
	/* Expand @-codes */
	txt=txt.replace(/@(.*)@/g,
		function (str, code, offset, s) {
			return bbs.atcode(code);
		}
	);
	/* ToDo: Expand \1D, \1T, \1<, \1Z */
	/* ToDo: "Expand" (ie: remove from string when appropriate) per-level/per-flag stuff */
	/* ToDo: Strip ANSI (I betcha @-codes can slap it in there) */

	while(p<txt.length && x<this.width && y<this.height) {
		ch=txt.substr(p++,1);
		switch(ch) {
			case '\1':		/* CTRL-A code */
				ch=txt.substr(p++,1);
				switch(ch) {
					case '\1':	/* A "real" ^A code */
						this.data[x][y].ch=ch;
						this.data[x][y].attr=curattr;
						x++;
						if(x>=this.width) {
							x=0;
							y++;
							if(scroll && y==this.height) {
								this.scroll(1);
								y--;
							}
						}
						break;
					case 'K':	/* Black */
						curattr=(curattr)&0x8f;
						break;
					case 'R':	/* Red */
						curattr=((curattr)&0xff)|RED;
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
						if(scroll && y==this.height) {
							this.scroll(1);
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
				y++;
				if(scroll && y==this.height) {
					this.scroll(1);
					y--;
				}
				break;
			default:
				this.data[x][y].ch=txt.substr(p,1);
				this.data[x][y].attr=curattr;
				x++;
				if(x>=this.width) {
					x=0;
					y++;
					if(scroll && y==this.height) {
						this.scroll(1);
						y--;
					}
				}
		}
	}
}
