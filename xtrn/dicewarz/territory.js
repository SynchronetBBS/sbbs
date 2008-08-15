function 	Territory(location)	
{								
	this.dice=1;
	this.player=-1;
	this.bColor=-1;
	this.fColor=-1;
	this.bfColor=-1;
	this.x=-1;
	this.y=-1;
	this.location=location;

	this.topLeft=">";
	this.topRight="<";
	this.bottomLeft=">";
	this.bottomRight="<";

	this.North=-1;
	this.South=-1;
	this.Northwest=-1;
	this.Northeast=-1;
	this.Southwest=-1;
	this.Southeast=-1;

										//OBJECT METHODS
	this.setBorder=			function(data)
	{										//CHECK PROXIMITY AND ASSIGN BORDER CHARACTERS
		proximity=ScanProximity(this.location);
		this.North=proximity[0];
		this.South=proximity[1];
		this.Northwest=proximity[2];
		this.Northeast=proximity[3];
		this.Southwest=proximity[4];
		this.Southeast=proximity[5];
		if(!data[this.North])
		{
			if(!data[this.Northwest]) this.topLeft=".";
			if(!data[this.Northeast]) this.topRight=".";
		}
		if(!data[this.South])
		{
			if(!data[this.Southwest]) this.bottomLeft="`";
			if(!data[this.Southeast]) this.bottomRight="'";
		}
	}
	this.assign=			function(number,player)
	{										//GIVE PLAYER OWNERSHIP OF THIS TERRITORY
		this.player=number;
		this.bColor=player.bColor;
		this.bfColor=player.bfColor;
		this.fColor=player.fColor;
	}
	this.show=				function()
	{										//DISPLAY THIS TERRITORY ON THE MAP
//		ALTERNATE MAP POSITION DISPLAY
//		display=(this.bfColor + ""+ blackBG + this.bColor + this.fColor + " " + this.dice + " " + blackBG + this.bfColor + "");

		display=(this.bfColor + "\xFE"+ blackBG + this.bColor + this.fColor + " " + this.dice + " " + blackBG + this.bfColor + "\xFE");
		console.gotoxy(this.x-1, this.y);
		console.putmsg(display);
	}
	this.displayBorder=		function(color)
	{										//DISPLAY THIS TERRITORY'S BORDER ON THE MAP
		console.gotoxy(this.x-2, this.y);	
		printf(color + "<");				
		console.gotoxy(this.x+4, this.y);
		printf(color + ">");
		console.gotoxy(this.x-1, this.y-1);
		printf(color+this.topLeft);
		DrawLine(color,3);
		printf(this.topRight);
		console.gotoxy(this.x-1, this.y+1);	
		printf(color+this.bottomLeft)
		DrawLine(color,3)
		printf(this.bottomRight);
	}
	this.displaySelected=	function(color)
	{										//DISPLAY THIS TERRITORY'S BORDER ON THE MAP
		console.gotoxy(this.x-2, this.y);	
		printf(color + "<");				
		console.gotoxy(this.x+4, this.y);
		printf(color + ">");
		console.gotoxy(this.x-1, this.y-1);
		printf(color+".");
		DrawLine(color,3);
		printf(color+".");
		console.gotoxy(this.x-1, this.y+1);	
		printf(color+"`")
		DrawLine(color,3)
		printf(color+"'");
	}
	this.getXY=				function(loc,sc,sr)
	{										//ASSIGN DISPLAY COORDINATES FOR MAP
		var startX=sc;
		var startY=sr;
		offset=loc%columns;
		if(offset%2==1) startY++;
		startX+=(offset*5);
		startY+=(parseInt(loc/columns)*2);
		this.x=startX;
		this.y=startY;
	}
	
	this.getXY(this.location,startColumn,startRow);
}
