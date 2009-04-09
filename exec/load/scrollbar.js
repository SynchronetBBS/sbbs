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
	this.wrap=false;
	this.color=color?color:'\1n';
	this.dark=(color=="\1k"?"\1k\1h":"\1n"+color);
	this.light=(color=="\1k"?"\1k\1h":color+"\1h");
	
	this.draw=function(index, range)
	{
		if(index>=0 && range>=0)
		{
			if(index>range) return; 	//invalid position = do not draw
			var percentage=index/range;
			this.index=Math.round(percentage*this.length);
			if(this.index==0) this.index=1;
		}		
		else if(this.index<1) return;	//no position supplied and no index = do not draw
		if(this.orientation=="vertical") this.drawVert();
		else this.drawHoriz();
	}
	this.drawVert=function()
	{
		console.gotoxy(this.x,this.y);
		console.putmsg(this.light + ascii(30));
		for(i=1;i<=this.length;i++)
		{
			if(this.x==80)
			{
				console.gotoxy(this.x,this.y+i);
			}
			else
			{
				console.down();
				console.left();
			}
			if(i==this.index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg(this.dark + '\xB0');
		}
		if(this.x==80)
		{
			console.gotoxy(this.x,this.y+this.length+1);
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
		console.gotoxy(this.x,this.y);
		console.putmsg(this.light + ascii(17));
		for(i=1;i<=this.length;i++)
		{
			if(i==this.index) console.putmsg(this.color + '\1h\xDB');
			else console.putmsg(this.dark + '\xB0');
		}
		console.putmsg(this.light + ascii(16));
	}
}