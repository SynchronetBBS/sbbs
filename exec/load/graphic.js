// $Id$

/*
 * "Graphic" object
 * Allows a graphic to be stored in memory and portions of it redrawn on command
 */

function Graphic(width,height,attribute,char)
{
	if(char==undefined)
		this.char=' ';
	else
		this.char=char;
	if(attribute==undefined)
		this.attribute=7;
	else
		this.attribute=attribute;
	if(height==undefined)
		this.height=24;
	else
		this.height=height;
	if(width==undefined)
		this.width=80;
	else
		this.width=width;

	this.data=new Array(width,height);
	var x;
	var y;
	for(y=0; y<this.height; y++) {
		for(x=0; x<this.width; x++) {
			this.data[x][y]=new Object;
			this.data[x][y].char=char;
			this.data[x][y].attr=attribute;
		}
	}
	this.draw=Graphic_draw;
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
	if(xpos+width > console.screen_columns || ypos+height > console.screen_rows) {
		alert("Attempt to draw outside of screen");
		return(false);
	}
	for(y=0;y<height; y++) {
		gotoxy(xpos,ypos+y);
		for(x=0; x<width; x++) {
			// Do not draw to the bottom left corner of the screen-would scroll
			if(xpos+x != console.screen_columns
					|| ypos+y != console.screen_rows) {
				console.attributes=this.data[x+xoff][y+yoff].attr;
				console.write(this.data[x+xoff][y+yoff].char;
			}
		}
	}
	return(true);
}
