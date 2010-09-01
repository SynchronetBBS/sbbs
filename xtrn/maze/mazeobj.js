//GAME OBJECTS
function UserList()
{
	this.x=58;
	this.y=4;
	this.list=[];
	this.draw=function()
	{
		for(var i in this.list)
		{
			console.gotoxy(this.x,this.y+i);
			console.putmsg(this.color+this.list[i].name);
		}
	}
}
function User(name)
{
	this.name=name;
	this.joined=false;
}
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
	this.prefix=system.qwk_id + "."; //BBS prefix FOR INTERBBS PLAY?? WE'LL SEE
	this.players=[];
	this.current;
	
	this.StorePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(root + "players.ini");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.close();
	}
	this.loadPlayers=function()
	{
		var pFile=new File(root + "players.ini");
		pFile.open(file_exists(pFile.name) ? "r+":"w+"); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name"))
		{
			pFile.iniSetObject(this.prefix + user.alias,new Player(user.alias));
		}
		var players=pFile.iniGetSections();
		for(player in players)
		{
			this.players[players[player]]=pFile.iniGetObject(players[player]);
			log("Loaded player: " + this.players[players[player]].name);
		}
		pFile.close();
	}
	this.GetFullName=function(name)
	{
		return(this.prefix + name);
	}

	function Player(name,besttime,wins)
	{
		this.name=name?name:user.alias;
		this.besttime=besttime?besttime:0;
		this.wins=wins?wins:0;
	}
	this.loadPlayers();
}
function Maze(rootFile,gameNumber)
{
	this.pcolors=["\1y","\1g","\1m","\1c","\1r","\1w"];	//LIST OF COLORS USED BY OTHER PLAYERS
	this.maze=new Graphic(79,22);
	this.timer=new Timer("\1y\1h");
	this.mazeFile=rootFile+".maz";
	this.dataFile=rootFile+".ini";
	this.gameNumber=gameNumber;

	this.players=[];
	this.start=false;
	this.finish=false;
	this.lastupdate=time();
	this.inprogress=false;
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
		log(this.start);
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
					log("start: " + this.start.x + "," + this.start.y +
						" finish: " + this.finish.x + "," + this.finish.y);
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
		file.open('r',true);
		var players=file.iniGetKeys("players");
		var created=file.iniGetValue(null,"created");
		var inprogress=file.iniGetValue(null,"inprogress");
		file.close();
		
		for(var p in players) {
			log("loading player: " + players[p]);
			if(!this.players[players[p]]) {
				this.players[players[p]]=new Player(players[p],this.pcolors.shift(),100);
			}
			this.goToStartingPosition(this.players[players[p]]);
		}
		this.lastupdate=time();
		this.inprogress=inprogress;
		
		var current=system.timer;
		var difference=current-created;
		this.timer.init(60-difference);
	}
	this.init();
}
function Coords(x,y)
{
	this.x=x;
	this.y=y;
}
function Player(name,color,health)
{
	this.name=name;
	this.color=color;
	this.health=health;
	this.coords;
	
	this.draw=function()
	{
		console.gotoxy(this.coords);
		console.putmsg(this.color + "\1h\xEA");
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