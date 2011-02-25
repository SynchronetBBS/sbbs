/*
	generic scrollbar for Synchronet
	3/24/2009 - Matt Johnson (MCMLXXIX)
*/

function Scrollbar(x,y,length,orientation,color)
{
	this.index;
	this.x=x;
	this.y=y;
	this.orientation=orientation;
	this.length=length-2;
	this.color=color?color:'';
	
	this.draw=function(index, range)
	{
		if(index>range) 
			return;
		if(!index || !range) {
			if(!this.index && !this.bar)
				return;
		}
		else if(isNaN(index) || isNaN(range))
			return;
		this.index=Math.ceil(((index+1)/range)*this.length);
		this.bar=Math.ceil((this.length/range)*this.length);
		if(this.orientation == "vertical")
			this.drawVert(this.index,this.index+this.bar);
		else 
			this.drawHoriz(this.index,this.index+this.bar);
	}
	this.drawVert=function(s,f)
	{
		console.gotoxy(this);
		console.putmsg('\1n' + this.color + ascii(30),P_SAVEATR);
		var ch='\xB0';
		for(var i=1;i<=this.length;i++)	{
			console.gotoxy(this.x,this.y+i);
			if(i == s) 
				ch='\1h\xDB';
			else if(i > f) 
				ch='\xB0';
			console.putmsg(ch,P_SAVEATR);
		}
		console.gotoxy(this.x,this.y+this.length+1);
		console.putmsg(ascii(31),P_SAVEATR);
	}
	this.drawHoriz=function(s,f)
	{
		if(f > this.length) {
			s--;
			f--;
		}
		console.gotoxy(this);
		console.putmsg('\1n' + this.color + ascii(17),P_SAVEATR);
		for(i=1;i<=this.length;i++)	{
			if(i == s) 
				ch='\1h\xDB';
			else if(i == f+1)
				ch='\xB0';
			console.putmsg(ch,P_SAVEATR);
		}
		console.putmsg(ascii(16),P_SAVEATR);
	}
}