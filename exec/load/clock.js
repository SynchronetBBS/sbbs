function DigitalClock(x,y,color)
{
	this.x=x;
	this.y=y;
	this.color=color;
	this.columns=17;
	this.rows=5;
	this.lastupdate=-1;
	this.digits=[];
	
	this.Init=function()
	{
		this.LoadDigits();
		this.DrawBox();
 		this.Update();
	}
	this.Update=function(forced)
	{
		var date=new Date();
		var hours=date.getHours();
		if(hours>12) hours-=12;
		if(hours<10) 
		{
			hours="  " + hours;
		}
		var minutes=date.getMinutes();
		if(minutes<10) minutes="0"+minutes;
		var time=hours.toString() + ":" + minutes.toString();
		if(minutes>this.lastupdate || forced) 
		{
			this.lastupdate=minutes;
			this.DrawClock(time);
		}
	}
	this.DrawClock=function(time)
	{
		console.attributes=this.color;
		for(i=0;i<5;i++)
		{
			console.gotoxy(this.x+1,this.y+1+i);
			for(d=0;d<time.length;d++)
			{
				var digit=time.charAt(d);
				if(digit==":") digit=this.digits["colon"][i] + " ";
				else if(digit==" ");
				else if(digit==1) digit=" " + this.digits[1][i] + " ";
				else digit=this.digits[parseInt(digit)][i] + " ";
				console.write(digit);
			}
		}
		console.attributes=ANSI_NORMAL;
	}
	this.Redraw=function()
	{
		this.DrawBox();
		this.Update(true);
	}
	this.DrawBox=function()
	{
		console.gotoxy(this.x,this.y);
		console.putmsg("\1n\1h\xDA\xB4\1nTIME\1h\xC3");
		DrawLine(false,false,this.columns-6,"\1n\1h");
		console.putmsg("\1n\1h\xBF");
		for(line = 1; line<=this.rows; line++)
		{
			console.gotoxy(this.x,this.y+line);
			printf("\1n\1h\xB3%*s\xB3",this.columns,"");
		}
		console.gotoxy(this.x,this.y + this.rows+1);
		console.putmsg("\1n\1h\xC0");
		DrawLine(false,false,this.columns,"\1n\1h");
		var spot=console.getxy();
		if(spot.y==24 && spot.x==80);
		else console.putmsg("\1n\1h\xD9");
	}
	this.LoadDigits=function()
	{
		var zero=["\xDC\xDC\xDC","\xDB \xDB","\xDB \xDB","\xDB \xDB","\xDF\xDF\xDF"];
		var one=["\xDC","\xDB","\xDB","\xDB","\xDF"];
		var two=["\xDC\xDC\xDC","  \xDB","\xDB\xDF\xDF","\xDB  ","\xDF\xDF\xDF"];
		var three=["\xDC\xDC\xDC","  \xDB","\xDF\xDF\xDB","  \xDB","\xDF\xDF\xDF"];
		var four=["\xDC \xDC","\xDB \xDB","\xDF\xDF\xDB","  \xDB","  \xDF"];
		var five=["\xDC\xDC\xDC","\xDB  ","\xDF\xDF\xDB","  \xDB","\xDF\xDF\xDF"];
		var six=["\xDC\xDC\xDC","\xDB  ","\xDB\xDF\xDB","\xDB \xDB","\xDF\xDF\xDF"];
		var seven=["\xDC\xDC\xDC","  \xDB","  \xDB","  \xDB","  \xDF"];
		var eight=["\xDC\xDC\xDC","\xDB \xDB","\xDB\xDF\xDB","\xDB \xDB","\xDF\xDF\xDF"];
		var nine=["\xDC\xDC\xDC","\xDB \xDB","\xDF\xDF\xDB","  \xDB","  \xDF"];
		var colon=[" ","\xFE"," ","\xFE"," "];
		
		this.digits[0]=zero;
		this.digits[1]=one;
		this.digits[2]=two;
		this.digits[3]=three;
		this.digits[4]=four;
		this.digits[5]=five;
		this.digits[6]=six;
		this.digits[7]=seven;
		this.digits[8]=eight;
		this.digits[9]=nine;
		this.digits["colon"]=colon;
	}
	this.Init();
}
