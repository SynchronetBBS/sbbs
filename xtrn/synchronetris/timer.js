function Timer(color)
{
	this.color=color?color:"\1n\1h";
	this.columns=17;
	this.rows=5;
	this.countdown=false;
	this.lastupdate=-1;
	this.lastshown=-1;
	
	this.init=function(length)
	{
		this.countdown=length;
		this.lastupdate="" + time();
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
			this.lastshown=current;
			var t=mins.toString() + ":" + secs.toString();
			this.drawClock(t);
		}
	}
	this.drawClock=function(time,x,y)
	{
		console.gotoxy(x,y);
		console.putmsg(this.color + time);
		console.attributes=ANSI_NORMAL;
	}
	this.countDown=function()
	{
		var difference=time()-this.lastupdate;
		this.countdown-=difference;
		
		if(this.countdown<=0) return false;
		this.lastupdate="" + time();
		return true;
	}
}
