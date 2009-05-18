function Window(name,x,y,w,h)
{
	this.name=name?name:false;
	this.x=x;
	this.y=y;
	this.columns=w;
	this.rows=h;
	this.active=false;
	this.Draw=function(value)
	{
		var title="";
		var color="";
		if(this.name)
		{
			color=(this.active?"\1c\1h":"\1n");		
			title="\1n\1h\xB4" + color + this.name + (value?": " + value:"") + "\1n\1h\xC3";
		}
		console.gotoxy(this.x,this.y);
		console.putmsg("\1n\1h\xDA");
		DrawLine(false,false,this.columns-console.strlen(title),"\1n\1h");
		console.putmsg(title + "\1n\1h\xBF");
		for(line = 1; line<=this.rows; line++)
		{
			console.gotoxy(this.x,this.y+line);
			printf("\1n\1h\xB3%*s\xB3",this.columns,"");
		}
		console.gotoxy(this.x,this.y + this.rows+1);
		console.putmsg("\1n\1h\xC0");
		DrawLine(false,false,this.columns,"\1n\1h");
		var spot=console.getxy();
		if(!(spot.y==console.screen_rows && spot.x==console.screen_columns)) console.putmsg("\1n\1h\xD9");
	}
}
