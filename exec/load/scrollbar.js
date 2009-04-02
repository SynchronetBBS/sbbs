/*
	generic scrollbar for Synchronet
	3/24/2009 - Matt Johnson (MCMLXXIX)
*/

function Scrollbar(firstx,firsty,lastx,lasty,color)
{
	this.first;
	this.last;
	this.orientation;
	this.index;
	this.length;
	this.color=color?color:'\1n';
	
	this.init=function(fx,fy,lx,ly)
	{
		this.first={'x':fx,'y':fy};
		this.last={'x':lx,'y':ly};
		
		if(fx==lx)
		{
			this.orientation=1;
			this.length=this.last.y-this.first.y-1;
		}
		else if(fy==ly) 
		{
			this.orientation=0;
			this.length=this.last.x-this.first.x-1;
		}
		else exit(); //if scrollbar orientation is not vertical or horizontal do not continue
	}
	this.increase=function()
	{
		if(this.index<this.length) this.index++;
		else if(this.wrap) this.index=0;
		this.draw();
	}
	this.decrease=function()
	{
		if(this.index>0) this.index--;
		else if(this.wrap) this.index=this.length;
		this.draw();
	}
	this.end=function()
	{
		this.index=this.length;
		this.draw();
	}
	this.home=function()
	{
		this.index=0;
		this.draw();
	}
	this.draw=function(position, total)
	{
		if(!this.first || !this.last) return; 		//if no start or end points initialized, do not draw
		if(position>=0 && total>=0)
		{
			if(position>total) return; 	//invalid position = do not draw
			var percentage=position/total;
			this.index=parseInt(percentage*this.length);
		}		
		else if(!this.index>=0) return;	//no position supplied and no index = do not draw
		
		if(this.orientation==1) this.drawVert();
		else this.drawHoriz();
	}
	this.drawVert=function()
	{
		if(this.length==0) return; //who needs a zero length scrollbar?
		
		console.gotoxy(this.first.x,this.first.y);
		console.putmsg(this.color + "\1h" + ascii(30));
		for(i=0;i<this.length;i++)
		{
			console.down();
			console.left();
			if(i==this.index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg('\1n' + this.color + '\xB0');
		}
		console.down();
		console.left();
		console.putmsg(this.color + "\1h" + ascii(31));
	}
	this.drawHoriz=function()
	{
		if(this.length==0) return; //who needs a zero length scrollbar?
		
		console.gotoxy(this.first.x,this.first.y);
		console.putmsg(this.color + "\1h" + ascii(17));
		for(i=1;i<=this.length;i++)
		{
			if(i==this.index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg('\1n' + this.color + '\xB0');
		}
		console.putmsg(this.color + "\1h" + ascii(16));
	}
	if(firstx && firsty && lastx && lasty) 
	{
		this.init(firstx,firsty,lastx,lasty);
	}
}