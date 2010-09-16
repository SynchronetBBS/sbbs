//GAME OBJECTS
function ScoreList()
{
	this.scores;
	this.SaveScores=function()
	{
	}
	this.loadScores=function()
	{
	}
	this.SortScores=function()
	{ 
	}	
}
function PlayerList()
{
	this.players=[];
	this.prefix=system.qwk_id + ".";
	
	this.storePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(root + "players.ini");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"besttime",player.losses);
		pFile.close();
		sendFiles(pFile.name);
	}
	this.loadPlayers=function()
	{
		var update=false;
		var pFile=new File(root + "players.ini");
		pFile.open(file_exists(pFile.name) ? "r+":"w+",true); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name")) {
			pFile.iniSetObject(this.prefix + user.alias,new Score(user.alias));
			update=true;
		}
		var players=pFile.iniGetSections();
		for(player in players)	{
			this.players[players[player]]=pFile.iniGetObject(players[player]);
		}
		pFile.close();
		if(update) sendFiles(pFile.name);
	}
	this.getPlayer=function(id)
	{
		return this.players[id];
	}
	this.getAlias=function(id)
	{
		return id.substr(id.indexOf(".")+1);
	}
	this.getPlayerID=function(name)
	{
		return(this.prefix + name);
	}
}
function Maze(rootFile,gameNumber)
{
	this.colors=["\1y","\1g","\1m","\1c","\1r","\1w"];	//LIST OF COLORS USED BY OTHER PLAYERS
	this.maze=new Graphic(79,22);
	this.timer=new Timer("\1y\1h");
	this.mazeFile=rootFile+".maz";
	this.dataFile=rootFile+".ini";
	this.gameNumber=gameNumber;

	this.players=[];
	this.start=false;
	this.finish=false;
	this.lastupdate=-1;
	this.status=-1;
	this.winner=false;
	this.damage=true;
		
	this.init=function()
	{
		if(!file_exists(this.mazeFile)) {
			generator.generate(this.dataFile,this.mazeFile);
		}
		this.loadMaze();
		this.findStartFinish();
		this.loadData();
	}
	this.goToStartingPosition=function(player)
	{
		player.coords=new Coords(this.start.x,this.start.y);
	}
	this.findStartFinish=function()
	{
		for(var x=0;x<this.maze.data.length;x++) {
			for(var y=0;y<this.maze.data[x].length;y++)	{
				if(!this.start) if(this.maze.data[x][y].ch=="S") {
					this.start=new Coords(x+1,y+1);
					this.maze.data[x][y].ch=" ";
				}
				if(!this.finish) if(this.maze.data[x][y].ch=="X") {
					this.finish=new Coords(x+1,y+1);
					this.maze.data[x][y].attr=14;
				}
				if(this.start && this.finish) {
					return true;
				}
			}
		}
		log("unable to find start & finish");
		return false;
	}
	this.draw=function()
	{
		console.clear();
		this.maze.draw(1,1);
	}
	this.loadMaze=function()
	{
		this.maze.load(this.mazeFile);
	}
	this.loadData=function()
	{
		var file=new File(this.dataFile);
		this.lastupdate=file_date(file.name);
		file.open('r',true);
		var players=file.iniGetKeys("players");
		var created=file.iniGetValue(null,"created");
		var status=file.iniGetValue(null,"status");
		file.close();
		
		for(var p in players) {
			if(!this.players[players[p]]) {
				this.players[players[p]]=new Player(players[p],this.colors.shift(),100);
			}
			this.goToStartingPosition(this.players[players[p]]);
		}
		this.status=status;
		this.lastupdate=time();
		
		if(created > 0) {
			var current=time();
			var difference=current-created;
			this.timer.init(30-difference);
		}
	}
	this.init();
}
function Coords(x,y)
{
	this.x=x;
	this.y=y;
}
function Score(name,besttime,wins)
{
	this.name=name;
	this.besttime=besttime?besttime:0;
	this.wins=wins?wins:0;
}
function Player(name,color,health)
{
	this.name=name;
	this.color=color;
	this.health=health;
	this.coords;
	
	this.draw=function(c)
	{
		var color="";
		if(this.health>=75) color=this.color+"\1h";
		else if(this.health>=50) color=this.color;
		else if(this.health>=25) color="\1k\1h";
		else color="\1r";
		console.gotoxy(this.coords);
		console.putmsg(color + "\xEA");
		console.home();
	}
	this.unDraw=function()
	{
		console.gotoxy(this.coords);
		write(" ");
	}
}
function Packet(func)
{
	this.func=func;
}