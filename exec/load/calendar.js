/*

	Javascript Event Calendar 
	for Synchronet v3.15+
	by Matt Johnson (2009)
	
*/

//TODO: make event calendar handle events, as the name would suggest
//		example: highlight birthdays, league reset dates, etc...


Date.prototype.getDayName = function(day,shortName) 
{   
	var days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];   
	if(shortName) 
	{  
		if(day) return days[day].substr(0,3);
		return days[this.getDay()].substr(0,3);  
	} 
	else 
	{      
		if(day) return days[day];
		return days[this.getDay()];   
	}
}
Date.prototype.getMonthName = function() 
{   
	var months=['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
	return months[this.getMonth()]; 
}
Date.prototype.daysInMonth = function() 
{   
	return new Date(this.getFullYear(), this.getMonth()+1, 0).getDate()
}
function Calendar(x,y,fg,hl,sel)
{
	this.fg=fg?fg:"";
	this.hl=hl?hl:"\0012\1g\1h";
	this.sel=sel?sel:"\0011\1r\1h";
	this.x=x?x:1;
	this.y=y?y:1;
	this.highlights=[];

	const daynames="\1-" + this.fg + " Su Mo Tu We Th Fr Sa";
	const topline="\1-" + this.fg + "\xDA\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xC2\xC4\xC4\xBF";
	const middleline="\1-" + this.fg + "\xC3\xC4\xC4\xC5\xC4\xC4\xC5\xC4\xC4\xC5\xC4\xC4\xC5\xC4\xC4\xC5\xC4\xC4\xC5\xC4\xC4\xB4";
	const bottomline="\1-" + this.fg + "\xC0\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xC1\xC4\xC4\xD9";

	this.date=new Date();
	this.month=new Month(this.date.getMonthName(),this.date.getFullYear());
	this.daysinmonth=this.date.daysInMonth();
	this.selected=new Date().getDate();
	this.offset=new Date(this.date.getFullYear(), this.date.getMonth(), 1).getDay();    
	
	this.init=function()
	{
		var numDays = this.daysinmonth;
		var startDay = this.offset;     
		
		for(var i=0; i<startDay; i++) 
		{ 
			this.month.days.push(new Day());
		}
		var border=startDay;
		
		for(i=1; i<=numDays; i++) 
		{   
			this.month.days.push(new Day(this.date.getDayName(i),i));
			border++;      
		}     
		while((border++%7)!=0) 
		{  
			this.month.days.push(new Day());
		}     
	}
	this.draw=function()
	{
		var x=this.x;
		var y=this.y;
		console.gotoxy(x,y); y++;
		console.putmsg(daynames,P_SAVEATR);
		console.gotoxy(x,y); y++;
		console.putmsg(topline,P_SAVEATR);
		console.gotoxy(x,y);
		for(l=0;l<this.month.days.length;l++)
		{
			if(l>0 && (l%7)==0) 
			{
				console.putmsg("\1-" + this.fg + "\xB3",P_SAVEATR);
				y++;
				console.gotoxy(x,y);
				console.putmsg(middleline,P_SAVEATR);
				y++;
				console.gotoxy(x,y);
			}
			var day=this.month.days[l];
			var color="\1-" + this.fg + "\1h";
			if(day.number==this.selected) color=this.sel;
			else if(this.highlights[day.number]) color=this.hl;
			console.putmsg("\1-" + this.fg + "\xB3",P_SAVEATR);
			day.draw(color);
		}
		console.putmsg("\1-" + this.fg + "\xB3",P_SAVEATR);
		y++;
		console.gotoxy(x,y);
		console.putmsg(bottomline,P_SAVEATR);
		console.attributes=ANSI_NORMAL;
	}
	this.drawDay=function(num,col)
	{
		var color=col?col:"\1-" + this.fg + "\1h";
		if(!col && this.highlights[num]) color=this.hl;
		var index=num + this.offset -1;
		var day=this.month.days[index];
		var posx=this.x+1 + ((index%7)*3);
		var posy=this.y+2 + (parseInt(index/7)*2);
		console.gotoxy(posx,posy);
		day.draw(color);
	}
	this.selectDay=function(dir)
	{
		var k=dir?dir:undefined;
		var original=this.selected;
		while(1)
		{
			if(k)
			{
				switch(k)
				{
					case KEY_UP:
						if(this.selected>7) 
						{
							this.drawDay(this.selected);
							this.selected-=7;
							this.drawDay(this.selected,this.sel);
						}	
						break;
					case KEY_DOWN:
						if(this.selected+7<=this.daysinmonth) 
						{
							this.drawDay(this.selected);
							this.selected+=7;
							this.drawDay(this.selected,this.sel);
						}	
						break;
					case KEY_LEFT:
						if(this.selected>1) 
						{
							this.drawDay(this.selected);
							this.selected--;
							this.drawDay(this.selected,this.sel);
						}	
						break;
					case KEY_RIGHT:
						if(this.selected<this.daysinmonth) 
						{
							this.drawDay(this.selected);
							this.selected++;
							this.drawDay(this.selected,this.sel);
						}	
						break;
					case "\x1b":
						this.drawDay(this.selected);
						this.selected=original;
						this.drawDay(this.selected,this.sel);
						return false;
					case "\r":
						return true;
					default:
						break;
				}
			}
			k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
		}
	}
	this.init();
}
function Month(name,year)
{
	this.name=name;
	this.year=year;
	this.days=[];
}
function Day(name,num)
{
	this.name=name?name:"";
	this.number=num?num:false;
	this.draw=function(col)
	{
		var color=col?col:"\1-";
		if(this.number)
		{
			console.putmsg(this.number<10?color+"0"+this.number:color+this.number,P_SAVEATR);
		}
		else
		{
			console.putmsg(color + "  ");
		}
	}
}
