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
	this.wrap=false;
	this.color=color?color:'\1n';
	this.dark=(color=="\1k"?"\1k\1h":"\1n"+color);
	this.light=(color=="\1k"?"\1k\1h":color+"\1h");
	
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
		else if(this.wrap) this.index=1;
		this.draw();
	}
	this.decrease=function()
	{
		if(this.index>1) this.index--;
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
		this.index=1;
		this.draw();
	}
	this.draw=function(position, total)
	{
		if(!this.first || !this.last) 
		{
			Log("Scrollbar error: no start or end points supplied");
			return; 		//if no start or end points initialized, do not draw
		}
		if(position>=0 && total>=0)
		{
			if(position>total) return; 	//invalid position = do not draw
			var percentage=position/total;
			this.index=Math.round(percentage*this.length);
			if(this.index==0) this.index=1;
		}		
		else if(this.index<1) return;	//no position supplied and no index = do not draw
		
		if(this.orientation==1) this.drawVert();
		else this.drawHoriz();
	}
	this.drawVert=function()
	{
		if(this.length==0) return; //who needs a zero length scrollbar?
		
		console.gotoxy(this.first.x,this.first.y);
		console.putmsg(this.light + ascii(30));
		for(i=1;i<=this.length;i++)
		{
			if(this.first.x==80)
			{
				console.gotoxy(this.first.x,this.first.y+i);
			}
			else
			{
				console.down();
				console.left();
			}
			if(i==this.index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg(this.dark + '\xB0');
		}
		if(this.first.x==80)
		{
			console.gotoxy(this.last.x,this.last.y);
		}
		else
		{
			console.down();
			console.left();
		}
		console.putmsg(this.light + ascii(31));
	}
	this.drawHoriz=function()
	{
		if(this.length==0) return; //who needs a zero length scrollbar?
		
		console.gotoxy(this.first.x,this.first.y);
		console.putmsg(this.light + ascii(17));
		for(i=1;i<=this.length;i++)
		{
			if(i==this.index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg(this.dark + '\xB0');
		}
		console.putmsg(this.light + ascii(16));
	}
	if(firstx && firsty && lastx && lasty) 
	{
		this.init(firstx,firsty,lastx,lasty);
	}
}