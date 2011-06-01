function Timer(x,y,color)
{
	this.x=x?x:1;
	this.y=y?y:1;
	this.color=color?color:"\1n\1h";
	this.columns=17;
	this.rows=5;
	this.countdown;
	this.lastupdate=-1;
	this.lastshown=-1;
	this.digits=[];
	
	this.init=function(length)
	{
		this.loadDigits();
		this.countdown=length;
		this.lastupdate=time();
	}
	this.redraw=function()
	{
		this.update(true);
	}
	this.update=function(forced)
	{
		var current=time();
		if(current>this.lastupdate || forced) 
		{
			var mins=parseInt(this.countdown/60);
			var secs=this.countdown%60;
			if(secs<10) secs= "0" + secs;
			if(mins==0 && secs<30) this.color="\1r\1h";
			this.lastshown=current;
			var t=mins.toString() + ":" + secs.toString();
			this.drawClock(t);
		}
	}
	this.drawClock=function(time)
	{
		console.attributes=this.color;
		for(i=0;i<5;i++) {
			console.gotoxy(this.x,this.y+i);
			for(d=0;d<time.length;d++) {
				var digit=time.charAt(d);
				switch(digit) {
				case ":":
					digit=this.digits["colon"][i] + " ";
					break;
				case "1":
					digit=" " + this.digits[1][i] + " ";
					break;
				case "2":
				case "3":
				case "4":
				case "5":
				case "6":
				case "7":
				case "8":
				case "9":
				case "0":
					digit=this.digits[parseInt(digit,10)][i] + " ";
					break;
				}
				console.write(digit);
			}
		}
		console.attributes=ANSI_NORMAL;
	}
	this.loadDigits=function()
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
	this.countDown=function(current,difference)
	{
		if(this.countdown<=0) return false;
		this.countdown-=difference;
		this.lastupdate=current;
		return true;
	}
}
