//PLAYER OBJECTS
function Player(name,sys_name,vote)
{
	this.name=name;
	this.system=sys_name;
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
	function compressScores()
	{
		compressed=[];
		for(var s in players.scores)
		{
			compressed.push(players.scores[s]);
		}
		return compressed;
	}
	function sortScores()
	{ 
		var scores=compressScores();
		var numScores=scores.length;
		for(n=0;n<numScores;n++)
		{
			for(m = 0; m < (numScores-1); m++) 
			{
				if(scores[m].points < scores[m+1].points) 
				{
					holder = scores[m+1];
					scores[m+1] = scores[m];
					scores[m] = holder;
				}
			}
		}
		return scores;
	}
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
			score.points=Number(score.points);
			score.kills=Number(score.kills);
			score.wins=Number(score.wins);
			score.losses=Number(score.losses);
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
		var data=new Packet("score");
		data.score=score;
		stream.send(data);
	}
	this.scoreKill=function(plyr)
	{
		this.load();
		var name=plyr.name;
		var sys_name=plyr.system;
		if(!this.scores[name]) this.scores[name]=new Score(name,sys_name);
		this.scores[name].kills++;
		this.scores[name].points+=Number(settings.point_set.kill);
		this.save(name);
	}
	this.scoreWin=function(plyr)
	{
		this.load();
		var name=plyr.name;
		var sys_name=plyr.system;
		if(!this.scores[name]) this.scores[name]=new Score(name,sys_name);
		this.scores[name].wins++;
		this.scores[name].points+=Number(settings.point_set.win);
		this.save(name);
	}
	this.scoreLoss=function(plyr)
	{
		this.load();
		var name=plyr.name;
		var sys_name=plyr.system;
		if(!this.scores[name]) this.scores[name]=new Score(name,sys_name);
		this.scores[name].losses++;
		this.scores[name].points+=Number(settings.point_set.loss);
		if(this.scores[name].points<settings.min_points) this.scores[name].points=settings.min_points;
		this.save(name);
	}
	this.scoreForfeit=function(plyr)
	{
		this.load();
		var name=plyr.name;
		var sys_name=plyr.system;
		if(!this.scores[name]) this.scores[name]=new Score(name,sys_name);
		this.scores[name].losses++;
		this.scores[name].points+=Number(settings.point_set.forfeit);
		if(this.scores[name].points<settings.min_points) this.scores[name].points=settings.min_points;
		this.save(name);
	}
	
	this.load();
}
function Score(name,sys_name)
{
	this.name=name;
	this.system=sys_name;
	this.kills=0;
	this.wins=0;
	this.losses=0;
	this.points=0;
}
