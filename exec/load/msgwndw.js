load("graphic.js");

function Window(x,y,w,h)
{
	this.title="";
	this.x=x;
	this.y=y;
	this.width=w;
	this.height=h;
	
	this.draw=function(color)
	{
		color=color?color:DARKGRAY;
		console.attributes=color;
		console.gotoxy(this);
		console.pushxy();
		console.putmsg("\xDA",P_SAVEATR);
		if(console.strlen(this.title)>0) {
			
			drawLine(false,false,this.width-4-console.strlen(this.title));
			console.putmsg("\xB4",P_SAVEATR);
			console.putmsg(this.title,P_SAVEATR);
			console.attributes=color;
		} else {
			drawLine(false,false,this.width-2);
		}
		console.putmsg("\xC3\xBF",P_SAVEATR);
		var line=2;
		while(line++<this.height)
		{
			console.popxy();
			console.down();
			console.pushxy();
			printf("\xB3%*s\xB3",this.width-2,"");
		}
		console.popxy();
		console.down();
		console.putmsg(printPadded("\xC0",this.width-1,"\xC4"),P_SAVEATR);
		var spot=console.getxy();
		if(spot.y!=console.screen_rows && spot.x!=console.screen_columns) console.putmsg("\xD9",P_SAVEATR);
	}
	this.clear=function()
	{
		clearBlock(this.x,this.y,this.columns,this.rows);
	}
	this.init=function(title,subtitle)
	{
		var name="";
		if(title)	{
			name=title;
			if(subtitle) {
				name+="\1n:"+subtitle;
			}
		}
		this.title=name;
	}
}
