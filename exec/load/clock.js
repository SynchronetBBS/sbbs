function DigitalClock()
{
	this.width=17;
	this.height=6;
	this.bg=BG_BLACK;
	this.fg=BLUE;
	this.lastupdate=-1;
	this.digits=[];
	this.hidden=false;
	
	this.days=[
		"Sun",
		"Mon",
		"Tues",
		"Weds",
		"Thurs",
		"Fri",
		"Sat"
	];
	this.months=[
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sept",
		"Oct",
		"Nov",
		"Dec"
	];
	
	this.init=function(x,y,w,fg,bg)
	{
		if(x) this.x=x;
		if(y) this.y=y;
		if(w) this.width=w;
		if(fg != undefined) this.fg=fg;
		if(bg != undefined) this.bg=bg;
		this.loadDigits();
	}
	this.hide=function()
	{
		this.hidden=true;
	}
	this.unhide=function()
	{
		this.hidden=false;
		this.redraw();
	}
	this.update=function()
	{
		if(this.hidden) return false;
		var diff=(time()-this.lastupdate);
		if(diff >= 1) {
			return true;
		}
	}
	this.draw=function(x,y,date)
	{
		if(date == undefined) date=new Date();
		if(x == undefined) x=1;
		if(y == undefined) y=1;
		var timestr=date.toLocaleTimeString();
		console.gotoxy(x,y);
		console.pushxy();
		console.attributes=this.bg + BLACK;
		console.write(printPadded("",this.width,"\xDF"));
		console.attributes=this.bg + this.fg;
		
		var gap=this.width-14;
		var offset=parseInt(gap/2,10);
		
		for(i=0;i<3;i++)
		{
			console.popxy();
			console.down();
			console.pushxy();
			console.write(printPadded("",offset));
			for(d=0;d<timestr.length;d++)
			{
				var digit=timestr[d];
				if(digit==":") {
					if(i == 1) console.write(" ");
					else console.write("\xFA");
					continue;
				}
				digit=this.digits[parseInt(digit)][i];
				console.write(digit);
			}
			console.write(printPadded("",gap-offset));
		}
		console.popxy();
		console.down();
		console.pushxy();
		console.write(splitPadded(" " + this.days[date.getDay()] + ",",
			this.months[date.getMonth()] + ". " + date.getDate() + " ",this.width));
		console.popxy();
		console.down();
		console.attributes=this.bg + BLACK;
		console.write(printPadded("",this.width,"\xDC"));
		console.attributes=ANSI_NORMAL;
		console.home();
		this.lastupdate="" + time();
	}
	this.loadDigits=function()
	{
		var zero=[	
			"\xDA\xB7",
			"\xB3\xBA",
			"\xC0\xBD"
		];
		var one=[
			" \xB7",
			" \xBA",
			" \xD0"
		];
		var two=[
			"\xDA\xB7",
			"\xDA\xBD",
			"\xC0\xBD"
		];
		var three=[
			"\xDA\xB7",
			" \xB6",
			"\xC0\xBD"
		];
		var four=[
			" \xD6",
			"\xC0\xB6",
			" \xD0"
		];
		var five=[
			"\xDA\xB7",
			"\xC0\xB7",
			"\xC0\xBD"
		];
		var six=[
			"\xDA\xB7",
			"\xC3\xB7",
			"\xC0\xBD"
		];
		var seven=[
			"\xDA\xB7",
			" \xBA",
			" \xD0"
		];
		var eight=[
			"\xDA\xB7",
			"\xC3\xB6",
			"\xC0\xBD"
		];
		var nine=[
			"\xDA\xB7",
			"\xC0\xB6",
			"\xC0\xBD"
		];
		
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
	}
}
