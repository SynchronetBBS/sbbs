//GAME OBJECTS
function GameData() 
{
	this.profiles={};
	this.games={};
	this.mazes={};
	this.online={};
	this.updated=true;
	
	this.loadGames=function() {
		this.games = client.read("mazerace","games",1);
		if(!this.games)
			this.games = {};
	}
	this.loadPlayers=function() {
		this.profiles=client.read(game_id,"profiles",1);
		if(!this.profiles)
			this.profiles = {};
		
		var alias = user.alias.replace(/\./g,"_");
		if(!this.profiles[alias]) {
			var p = new Profile(alias);
			this.profiles[p.name] = p;
			client.write("mazerace","profiles."+p.name,p,2);
		}
	}
	this.who=function() {
		this.online=client.who(game_id,"games");
	}
	this.storeGameStatus=function(gameNumber,status) {
		this.games[gameNumber].status = status;
		client.write(game_id,
			"games." + 
			gameNumber + 
			".status", 
			status,2);
	}
	this.storeRaceWinner=function(gameNumber,winner) {
		this.games[gameNumber].winner = winner;
		this.profiles[winner].wins++;
		client.write(game_id,
			"games." + 
			gameNumber + 
			".winner", 
			winner,2);
		client.write(game_id,
			"profiles." +
			winner +
			".wins",
			this.profiles[winner].wins,2);
	}
	this.storeRaceTime=function(gameNumber,raceTime,player) {
		this.games[gameNumber].raceTime = raceTime;
		if(raceTime < this.profiles[player].best_time || this.profiles[player].best_time == 0) {
			this.profiles[player].best_time = raceTime;
			client.write(game_id,
				"profiles." +
				player +
				".best_time",
				raceTime,2);
		}
	}
	this.storePlayerPosition=function(gameNumber,player) {
		this.mazes[gameNumber].players[player.name].coords = player.coords;
		client.write(game_id,
			"mazes." + 
			gameNumber + 
			".players." + 
			player.name + ".coords",
			player.coords,2);
	}
	this.storePlayerHealth=function(gameNumber,player) {
		this.mazes[gameNumber].players[player.name].health = player.health;
		client.write(game_id,
			"mazes." + 
			gameNumber + 
			".players." + 
			player.name  + ".health",
			player.health,2);
	}
	this.addPlayer=function(gameNumber,profile) {
		var player = new Player(profile.name,profile.avatar,profile.color);
		this.games[gameNumber].players[player.name] = player;
		client.write(game_id,
			"games." + 
			gameNumber + 
			".players." + 
			player.name,
			player,2);
	}
	this.delPlayer=function(gameNumber,profile) {
		delete this.games[gameNumber].players[profile.name];
		client.remove(game_id,
			"games." + 
			gameNumber + 
			".players." + 
			profile.name,
			2);
	}
	
	this.loadGames();
	this.loadPlayers();
	this.who();
}
function Coords(x,y)
{
	this.x=x;
	this.y=y;
}
function Profile(name) 
{
	this.name=name;
	this.avatar=settings.avatars[0];
	this.color=settings.colors[0];
	this.wins=0;
	this.losses=0;
	this.best_time=0;
}
function Player(name,avatar,color)
{
	this.name=name;
	this.coords={};
	this.health=100;
	this.avatar=settings.avatars[0];
	this.color=settings.colors[0];
	this.ready=false;
}
function Game(gameNumber) 
{
	this.gameNumber=gameNumber;
	this.players={};

	this.damage=false;
	this.fog=false;
	this.status=status.WAITING;
}
