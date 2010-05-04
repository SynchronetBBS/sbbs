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
	function countActivePlayers(map)
	{
		var count=0;
		for(var p=0;p<map.players.length;p++) {
			if(map.players[p].active) count++;
		}
		return count;
	}
	function getPlayerTiles(map,p)
	{
		var player_tiles=[];
		for(var t in map.tiles) {
			if(map.tiles[t].owner==p) {
				player_tiles.push(map.tiles[t]);
			}
		}
		return player_tiles;
	}
	function findPlayer(map,name) 
	{
		for(var p in map.players)
		{
			if(map.players[p].name==name) return p;
		}
		return -1;
	}
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

//PLAYER DATA OBJECTS
function PlayerData(filename)
{
	this.file=new File(filename);
	this.list=[];
	this.scores=[];
	this.load=function()
	{
		if(!this.file.is_open) {
			this.file.open(file_exists(this.file.name)?'r+':'w+');
		}
		var scores=this.file.iniGetAllObjects("name");
		this.file.close();
		for(var s in scores) {
			var score=scores[s];
			this.scores[score.name]=score;
		}
	}
	this.save=function(name)
	{
		var score=this.scores[name];
		if(!this.file.is_open) {
			this.file.open(file_exists(this.file.name)?'r+':'w+');
		}
		this.file.iniSetObject(score.name,score);
		this.file.close();
	}
	this.scoreKill=function(name)
	{
		this.load();
		if(!this.scores[name]) this.scores[name]=new Score(name);
		this.scores[name].kills++;
		this.scores[name].points+=parseInt(settings.point_set.kill,10);
		this.save(name);
	}
	this.scoreWin=function(name)
	{
		this.load();
		if(!this.scores[name]) this.scores[name]=new Score(name);
		this.scores[name].wins++;
		this.scores[name].points+=parseInt(settings.point_set.win,10);
		this.save(name);
	}
	this.scoreLoss=function(name)
	{
		this.load();
		if(!this.scores[name]) this.scores[name]=new Score(name);
		this.scores[name].losses++;
		this.scores[name].points+=parseInt(settings.point_set.loss,10);
		this.save(name);
	}
	this.scoreForfeit=function(name)
	{
		this.load();
		if(!this.scores[name]) this.scores[name]=new Score(name);
		this.scores[name].losses++;
		this.scores[name].points+=parseInt(settings.point_set.forfeit,10);
		this.save(name);
	}
}
function Score(name)
{
	this.name=name;
	this.kills=0;
	this.wins=0;
	this.losses=0;
	this.points=0;
}

	//PLAYER DATA FUNCTIONS
	function viewRankings()
	{
		clearBlock(2,2,78,16);
		console.gotoxy(2,2);
		
	}
	
