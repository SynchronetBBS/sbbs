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
		pFile.iniSetValue(id,"losses",player.losses);
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
function Battle(dataFile,mapFile)
{
	this.map=new Graphic(80,24);
	//this.mapFile;
	//this.dataFile;
	
	this.players=[];
	this.start=[];
	this.color=["\1w","\1c","\1y","\1r","\1m","\1g"];
	this.timer=new Timer("\1y\1h");

	this.lastupdate=-1;
	this.status=-1;
	this.winner=false;
	
	this.init=function()
	{
		if(dataFile) {
			this.dataFile=dataFile;
			this.loadData();
		} else {
			this.setFileInfo();
		}
		if(mapFile) {
			this.mapFile=mapFile;
			this.loadMap();
		}
		this.findStartingPositions();
	}
	this.setFileInfo=function()
	{
		var gNum=1;
		if(gNum<10) gNum="0"+gNum;
		while(file_exists(root + "battle" + gNum + ".ini")) {
			gNum++;
			if(gNum<10) gNum="0"+gNum;
		}
		var fName=root + "battle" + gNum + ".ini";
		this.dataFile=fName;
		this.gameNumber=parseInt(gNum,10);
	}
	this.draw=function()
	{
		this.map.draw();
	}
	this.findStartingPositions=function()
	{
		for(x in this.map.data) {
			for(y in this.map.data[x]) {
				if(this.map.data[x][y].ch=="X") 
				{
					this.map.data[x][y].ch=" ";
					this.start.push(new Coords(Number(x)+1,Number(y)+1));
				}
			}
		}
	}
	this.loadMap=function()
	{
		if(this.mapFile) this.map.load(this.mapFile);
	}
	this.loadData=function()
	{
		var file=new File(this.dataFile);
		file.open('r',true);

		this.gameNumber=Number(file.iniGetValue(null,"gameNumber"));
		this.status=file.iniGetValue(null,"status");

		var players=file.iniGetObject("players");
		var created=file.iniGetValue(null,"created");
		var mapFile=file.iniGetValue(null,"mapfile");
		
		file.close();
		
		if(mapFile) {
			this.mapFile=mapFile;
			this.loadMap();
			this.findStartingPositions();
		}
		for(var p in players) {
			log("loading player: " + p);
			if(!this.players[p]) {
				var position=players[p];
				var player=new Player(p,position,100);
				player.start=this.start[position];
				player.color=this.color[position];
				player.coords=new Coords(player.start.x,player.start.y);
				this.players[p]=player;
			}
		}
		if(created > 0) {
			var current=time();
			var difference=current-created;
			this.timer.init(30-difference);
		}
		this.lastupdate=time();
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
function Player(name,position,health)
{
	this.name=name;
	this.position=position;
	this.start;
	this.color;
	this.coords;
	this.health=health;
	this.heading=0;
	this.turret=0;
}
function Shot(heading,coords,range,player,color)
{
	this.heading=heading;
	this.coords=coords;
	this.player=player;
	this.color=color;
	this.range=range;
}
function Packet(func)
{
	this.func=func;
}
