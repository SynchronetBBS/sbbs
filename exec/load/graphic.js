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

	if(!(f.open("rb",true)))
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
		this.data[x][y].ch=txt.substr(p,1);
		if(attr!=undefined)
			this.data[x][y].attr=attr;
		x++;
		if(x>=this.width) {
			x=0;
			y++;
		}
	}
}
