function DigitalClock()
{
	this.x;
	this.y;
	this.color;
	this.columns=17;
	this.rows=5;
	this.lastupdate=-1;
	this.digits=[];
	this.hidden=false;
	this.box;
	
	this.Menu=function()
	{
		while(1) 
		{
			Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			switch(k)
			{
				case '\x12':	/* CTRL-R (Quick Redraw in SyncEdit) */
						Redraw();
						break;
				case ctrl('O'): /* CTRL-O - Pause */
				case ctrl('U'): /* CTRL-U User List */
				case ctrl('T'): /* CTRL-T Time Info */
				case ctrl('K'): /* CTRL-K Control Key Menu */
				case ctrl('P'): /* Ctrl-P Messages */
					controlkeys.handle(key);
					break;
				case '\x09':	/* CTRL-I TAB... ToDo expand to spaces */
				case KEY_RIGHT:
					NextWindow("clock");
				case "\x1b":
					return;
				default:
					break;
			}
		}
	}
	this.Init=function(x,y,color)
	{
		this.x=x?x:1;
		this.y=y?y:1;
		this.color=color?color:"\1n\1h";
		this.LoadDigits();
		this.box=new  Window("TIME",this.x,this.y,this.columns,this.rows);
		this.box.Draw();
 		this.Update(true);
	}
	this.Hide=function()
	{
		this.hidden=true;
	}
	this.Unhide=function()
	{
		this.hidden=false;
		this.Redraw();
	}
	this.Update=function(forced)
	{
		if(this.hidden) return;
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
			this.box.Draw();
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
		this.Update(true);
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
}
