/*
	generic scrollbar for Synchronet
	3/24/2009 - Matt Johnson (MCMLXXIX)
*/

function Scrollbar(firstx,firsty,lastx,lasty,color)
{
	this.first;
	this.last;
	this.orientation;
	this.color=color?color:'\1n';

	this.init=function(fx,fy,lx,ly)
	{
		this.first={'x':fx,'y':fy};
		this.last={'x':lx,'y':ly};
		
		if(fx==lx) 		this.orientation=1;
		else if(fy==ly) this.orientation=0;
		else exit(); //if scrollbar orientation is not vertical or horizontal do not continue
	}
	this.draw=function(position, total)
	{
		if(!this.first || !this.last) return; 		//if no start or end points initialized, do not draw
		if(!position || position>total) 
		{
			return;		//if position is invalid, do not draw
		}
		
		var percentage=position/total;
		
		if(this.orientation==1) this.drawVert(percentage);
		else this.drawHoriz(percentage);
	}
	this.drawVert=function(percentage)
	{
		var length=this.last.y-this.first.y-1;
		if(length==0) return; //who needs a zero length scrollbar?
		
		var index=parseInt(percentage*length);
		
		console.gotoxy(this.first.x,this.first.y);
		console.putmsg(this.color + "\1h" + ascii(30));
		for(i=1;i<=length;i++)
		{
			console.down();
			console.left();
			if(i==index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg('\1n' + this.color + '\xB0');
		}
		console.down();
		console.left();
		console.putmsg(this.color + "\1h" + ascii(31));
	}
	this.drawHoriz=function(percentage)
	{
		var length=this.last.x-this.first.x-1;
		if(length==0) return; //who needs a zero length scrollbar?
		
		var index=parseInt(percentage*length);
		
		console.gotoxy(this.first.x,this.first.y);
		console.putmsg(this.color + "\1h" + ascii(17));
		for(i=1;i<=length;i++)
		{
			if(i==index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg('\1n' + this.color + '\xB0');
		}
		console.putmsg(this.color + "\1h" + ascii(16));
	}
	if(firstx && firsty && lastx && lasty) 
	{
		this.init(firstx,firsty,lastx,lasty);
	}
}