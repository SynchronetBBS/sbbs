//PLAYER OBJECTS
function Player(name,vote)
{
	this.name=name;
	this.vote=vote;
	this.reserve=0;
	this.active=true;
	this.AI=false;
}
function AI(sort,check,qty)
{
	this.sort=sort
	this.check=check;
	this.qty=qty;
	this.turns=0;
	this.moves=0;
}
	//PLAYER FUNCTIONS
	function countDice(map,p)
	{
		var dice=0;
		var tiles=getPlayerTiles(map,p);
		for(var t=0;t<tiles.length;t++) {
			dice+=tiles[t].dice;
		}
		return dice;
	}
	function countTiles(map,p)
	{
		return getPlayerTiles(map,p).length;
	}

//PLAYER DATA OBJECT
function PlayerData(filename)
{
	this.list=[];
	this.scores=[];
	this.load=function()
	{
	
	}
}
	//PLAYER DATA FUNCTIONS
	function viewRankings()
	{

	}
	
