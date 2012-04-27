function	Player(userNumber, vote)
{
	this.user=userNumber;
	this.territories=[];
	this.totalDice=0;
	this.bColor="";
	this.bfColor="";
	this.fColor="";
	this.starting_territories=0;
	this.reserve=0;
	this.eliminated=false;
	this.vote=vote;
	this.AI={ name:"", sort:"Random", check:"Random", qty:"Random", turns:0, moves:0 };

	this.setColors=function(num)
	{
		this.bColor=console.ansi(bColors[num]);
		this.bfColor=console.ansi(bfColors[num]);
		this.fColor=fColors[num];
	}
	this.removeTerritory=function(territory)
	{
		for(var rem in this.territories) {
			if(this.territories[rem]==territory) 
				this.territories.splice(rem,1);
		}
	}
	this.countTerritory=function()
	{
		var count=0;
		for(var tt in this.territories)	
			count++;
		return count;
	}
}
