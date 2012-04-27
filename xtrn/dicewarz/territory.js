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
	this.bottomleft=">";
	this.bottomright="<";

	this.north=-1;
	this.south=-1;
	this.northwest=-1;
	this.northeast=-1;
	this.southwest=-1;
	this.southeast=-1;

										//OBJECT METHODS
	this.setBorder=			function(data)
	{										//CHECK PROXIMITY AND ASSIGN BORDER CHARACTERS
		proximity=scanProximity(this.location);
		this.north=proximity[0];
		this.south=proximity[1];
		this.northwest=proximity[2];
		this.northeast=proximity[3];
		this.southwest=proximity[4];
		this.southeast=proximity[5];
		if(!data[this.north]) {
			if(!data[this.northwest]) this.topLeft=".";
			if(!data[this.northeast]) this.topRight=".";
		}
		if(!data[this.south]) {
			if(!data[this.southwest]) this.bottomleft="`";
			if(!data[this.southeast]) this.bottomright="'";
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
		var display=(this.bfColor + "\xFE"+ blackbg + this.bColor + this.fColor + " " + this.dice + " " + blackbg + this.bfColor + "\xFE");
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
		drawLine(color,3);
		printf(this.topRight);
		console.gotoxy(this.x-1, this.y+1);	
		printf(color+this.bottomleft)
		drawLine(color,3)
		printf(this.bottomright);
	}
	this.displaySelected=	function(color)
	{										//DISPLAY THIS TERRITORY'S BORDER ON THE MAP
		console.gotoxy(this.x-2, this.y);	
		printf(color + "<");				
		console.gotoxy(this.x+4, this.y);
		printf(color + ">");
		console.gotoxy(this.x-1, this.y-1);
		printf(color+".");
		drawLine(color,3);
		printf(color+".");
		console.gotoxy(this.x-1, this.y+1);	
		printf(color+"`")
		drawLine(color,3)
		printf(color+"'");
	}
	this.getXY=				function(loc,sc,sr)
	{										//ASSIGN DISPLAY COORDINATES FOR MAP
		var startX=sc;
		var startY=sr;
		var offset=loc%columns;
		if(offset%2==1) startY++;
		startX+=(offset*5);
		startY+=(parseInt(loc/columns)*2);
		this.x=startX;
		this.y=startY;
	}
	
	this.getXY(this.location,startColumn,startRow);
}
