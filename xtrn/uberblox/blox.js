/*
	UBER BLOX!
	
	A javascript block puzzle game similar to GameHouse "Super Collapse" 
	by Matt Johnson (2009)

	for Synchronet v3.15+
*/

var gameroot;
try { barfitty.barf(barf); } catch(e) { gameroot = e.fileName; }
gameroot = gameroot.replace(/[^\/\\]*$/,"");

load("graphic.js");
load("sbbsdefs.js")
load("logging.js");
load("funclib.js");
 
var oldpass=console.ctrl_key_passthru;
//var gamelog=new Logger(gameroot,"blox");
var gamelog=false;

function Blox()
{
	const columns=20;
	const rows=11;
	const bg=[BG_GREEN,BG_BLUE,BG_RED,BG_BROWN,BG_MAGENTA,BG_CYAN];
	const fg=[LIGHTGREEN,LIGHTBLUE,LIGHTRED,YELLOW,LIGHTMAGENTA,LIGHTCYAN];

	var level;
	var points;
	var current;
	var scores;
	var players;
	var gameover=false;
	
	const points_base=			15;
	const points_increment=	10;
	const minimum_cluster=		3;
	const colors=				[3,3,3,3,3,4,4,4,4,5,5,5,6,6]; //COLORS PER MAP (DEFAULT ex.: levels 1-5 have 3 colors)
	const tiles_per_color=		[8,7,6,5,4,10,9,8,7,12,11,10,12,11];	//TARGET FOR LEVEL COMPLETION (DEFAULT ex. level 1 target: 220 total tiles minus 6 tiles per color times 3 colors = 202)
		
	var lobby;
	var logo;
	var gameend;
	var selx=0;
	var sely=0;
	
	var selection=[];
	var selected=false;
	var counted;
	const xlist=[0,0,1,-1];
	const ylist=[-1,1,0,0];

	function Lobby()
	{
		DrawLobby();
		while(1)
		{
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				switch(k.toUpperCase())
				{
					case "\x1b":	
					case "Q":
						SplashExit();
					case "I":
						ShowInstructions();
						DrawLobby();
						break;
					case "P":
						Main();
						DrawLobby();
						break;
					case "R":
						DrawLobby();
						break;
					default:
						break;
				}
			}
		}
	}
	function ShowInstructions()
	{
		console.clear(ANSI_NORMAL);
		bbs.sys_status&=~SS_PAUSEOFF;
		bbs.sys_status|=SS_PAUSEON;	
		var helpfile=gameroot + "help.doc";
		if(file_exists(helpfile)) console.printfile(helpfile);
		bbs.sys_status&=~SS_PAUSEON;
		bbs.sys_status|=SS_PAUSEOFF;	
	}
	function Main()
	{
		level=0;
		points=0;
		gameover=false;
		GenerateLevel();
		Redraw();
		while(1)
		{
			if(gameover) 
			{
				EndGame();
				while(console.inkey()=="");
				return;
			}
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				switch(k.toUpperCase())
				{
					case KEY_UP:
					case KEY_DOWN:
					case KEY_LEFT:
					case KEY_RIGHT:
						if(selected)
						{
							Unselect();
						}
						MovePosition(k);
						break;
					case "R":
						Redraw();
						break;
					case "\x1b":	
					case "Q":
						gameover=true;
						break;
					case "\r":
					case "\n":
						ProcessSelection();
						break;
					default:
						break;
				}
			}
		}
	}
	function DrawLobby()
	{
		lobby.draw(1,1);
		ShowScores();
	}
	function LevelUp()
	{
		level+=1;
		GenerateLevel();
	}
	function EndGame()
	{
		if(players.players[user.alias].score<points) 
		{
			players.players[user.alias].score=points;
			players.StorePlayer();
		}
		console.clear();
		gameend.draw();
		console.gotoxy(52,5);
		console.putmsg(CenterString("\1n\1y" + points,13));
		console.gotoxy(52,13);
		console.putmsg(CenterString("\1n\1y" + (parseInt(level)+1),13));
	}
	function SplashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status|=SS_PAUSEOFF;	
		console.clear();
	}
	function SplashExit()
	{
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		players.StorePlayer();
		console.clear(ANSI_NORMAL);
		exit(0);
	}
	function Init()
	{
		logo=new Graphic(18,22);
		logo.load(gameroot + "blox.bin");
		lobby=new Graphic(80,23);
		lobby.load(gameroot + "lobby.bin");
		gameend=new Graphic(80,23);
		gameend.load(gameroot + "gameend.bin");
		players=new PlayerList();
	}
	function GenerateLevel()
	{
		var numcolors=colors[level];
		var tiles=(columns*rows)-(numcolors*tiles_per_color[level]);
		
		grid=new Array(columns);
		for(var x=0;x<grid.length;x++)
		{
			grid[x]=new Array(rows);
			for(var y=0;y<grid[x].length;y++)
			{
				var col=random(numcolors);
				grid[x][y]=new Tile(bg[col],fg[col],x,y);
			}
		}
		current=new Level(grid,tiles);
	}
	function Redraw()
	{
		console.clear(ANSI_NORMAL);
		DrawGrid();
		DrawLogo();
		DrawInfo();
		ShowPosition();
	}
	function DrawGrid()
	{
		ClearBlock(1,1,60,23);
		for(var x=0;x<current.grid.length;x++)
		{
			for(var y=0;y<current.grid[x].length;y++)
			{
				current.grid[x][y].draw(false,x,y);
			}
		}
		console.attributes=ANSI_NORMAL;
	}
	function DrawLogo()
	{
		logo.draw(62,1);
		console.attributes=ANSI_NORMAL;
	}
	function DrawInfo()
	{
		ShowPlayer();
		ShowScore();
		ShowSelection(selection.length);
		ShowLevel();
		ShowTiles();
		console.gotoxy(1,24);
		console.putmsg("\1nArrow keys : move \1k\1h[\1nENTER\1k\1h]\1n : select \1k\1h[\1nQ\1k\1h]\1n : quit \1k\1h[\1n?\1k\1h]\1n : help \1k\1h[\1nR\1k\1h]\1n : redraw");
	}
	function Gotoxy(x,y)
	{
		var posx=(x*3)+2;
		var posy=22-(y*2);
		console.gotoxy(posx,posy);
	}
	function MovePosition(k)
	{
		current.grid[selx][sely].draw(false,selx,sely);
		switch(k)
		{
			case KEY_DOWN:
				if(sely>0) sely--;
				else sely=current.grid[selx].length-1;
				break;
			case KEY_UP:
				if(sely<current.grid[selx].length-1) sely++;
				else sely=0;
				break;
			case KEY_LEFT:
				if(selx>0) selx--;
				else selx=current.grid.length-1;
				if(sely>=current.grid[selx].length)	sely=current.grid[selx].length-1;		
				break;
			case KEY_RIGHT:
				if(selx<current.grid.length-1) selx++;
				else selx=0;
				if(sely>=current.grid[selx].length)	sely=current.grid[selx].length-1;		
				break;
		}
		ShowPosition()
	}
	function Grid(c,r)
	{
		var array=new Array(c);
		for(var cc=0;cc<array.length;cc++)
		{
			array[cc]=new Array(r);
		}
		return array;
	}
	function Select()
	{
		for(var s in selection)
		{
			var spot=selection[s];
			var tile=current.grid[spot.x][spot.y];
			tile.draw(true,spot.x,spot.y);
		}
		selected=true;
		ShowSelection(selection.length);
	}
	function Unselect()
	{
		for(var s in selection)
		{
			var spot=selection[s];
			var tile=current.grid[spot.x][spot.y];
			tile.draw(false,spot.x,spot.y);
		}
		selection=[];
		selected=false;
		ShowSelection(0);
	}
	function ProcessSelection()
	{
		if(selected) 
		{
			RemoveBlocks();
			if(!FindValidMove())
			{
				if(current.tiles<=0) 
				{
					mswait(1000);
					LevelUp();
					Redraw();
				}
				else gameover=true;
			}
		}
		else
		{
			counted=Grid(columns,rows);
			Search(selx,sely);
			if(selection.length>=minimum_cluster)			
			{
				Log("Selecting " + selection.length + " tiles");
				Select();
			}
			else
			{
				selection=[];
			}
		}
	}
	function Search(x,y)
	{
		var testcolor=current.grid[x][y].bg;
		counted[x][y]=true;
		selection.push(new Coord(x,y));
		
		for(var i=0;i<4;i++)
		{
			var spot=new Coord(x+xlist[i],y+ylist[i]);
			if(current.grid[spot.x] && !counted[spot.x][spot.y])
			{
				if(current.grid[spot.x][spot.y] && current.grid[spot.x][spot.y].bg==testcolor)
				{
					Search(spot.x,spot.y);
				}
			}
		}
	}
	function Coord(x,y)
	{
		this.x=x;
		this.y=y;
	}
	function SortSelection()
	{
		for(var n=0;n<selection.length;n++)
		{
			for(var m = 0; m < (selection.length-1); m++) 
			{
				if(selection[m].x < selection[m+1].x) 
				{
					var holder = selection[m+1];
					selection[m+1] = selection[m];
					selection[m] = holder;
				}
				else if(selection[m].x == selection[m+1].x) 
				{
					if(selection[m].y < selection[m+1].y) 
					{
						var holder = selection[m+1];
						selection[m+1] = selection[m];
						selection[m] = holder;
					}
				}
			}
		}
	}
	function RemoveBlocks()
	{
		Log("Removing " + selection.length + " blocks");
		
		SortSelection();
		
		for(var s=0;s<selection.length;s++)
		{
			var coord=selection[s];
			current.grid[coord.x].splice(coord.y,1);
			if(current.grid[coord.x].length==0) current.grid.splice(coord.x,1);
		}
		current.tiles-=selection.length;
		if(current.tiles<0) current.tiles=0;
		points+=CalculatePoints();
		selection=[];
		Unselect();
		ShowTiles();
		
		if(selx>=current.grid.length) selx=current.grid.length-1;
		if(sely>=current.grid[selx].length) sely=current.grid[selx].length-1;

		ShowScore();
		DrawGrid();
		ShowPosition();
	}
	function ShowPosition()
	{
		current.grid[selx][sely].draw(true,selx,sely);
	}
	function ShowPlayer()
	{
		console.gotoxy(63,9);
		console.putmsg("\1w\1h" + user.alias);
	}
	function ShowScore()
	{
		console.gotoxy(63,12);
		console.putmsg(PrintPadded("\1w\1h" + points,15));
	}
	function ShowSelection()
	{
		console.gotoxy(63,15);
		console.putmsg(PrintPadded("\1w\1h" + CalculatePoints(),15));
	}
	function ShowLevel()
	{
		console.gotoxy(63,18);
		console.putmsg("\1w\1h" + (parseInt(level)+1));
	}
	function ShowTiles()
	{
		console.gotoxy(63,21);
		console.putmsg(PrintPadded("\1w\1h" + current.tiles,15));
	}
	function CalculatePoints()
	{
		if(selection.length)
		{
			var p=points_base;
			for(var t=0;t<selection.length;t++)
			{
				p+=points_base+(t*points_increment);
			}
			return p;
		}
		return 0;
	}
	function FindValidMove()
	{
		counted=Grid(columns,rows);
		for(var x=0;x<current.grid.length;x++)
		{
			for(var y=0;y<current.grid[x].length;y++)
			{
				selection=[];
				Search(x,y); 
				if(selection.length>=minimum_cluster) 
				{
					selection=[];
					return true;
				}
			}
		}
		Log("no valid move found");
		return false;
	}
	function SortScores()
	{
		var sorted=[];
		for(var p in players.players)
		{
			var s=players.players[p];
			sorted.push(s);
		}
		var numScores=sorted.length;
		for(n=0;n<numScores;n++)
		{
			for(m = 0; m < (numScores-1); m++) 
			{
				if(sorted[m].score < sorted[m+1].score) 
				{
					holder = sorted[m+1];
					sorted[m+1] = sorted[m];
					sorted[m] = holder;
				}
			}
		}
		return sorted;
	}
	function ShowScores()
	{
		var posx=3;
		var posy=4;
		var index=0;
		
		var scores=SortScores();
		for(var s in scores)
		{
			var score=scores[s];
			if(score.score>0)
			{
				if(score.name==user.alias) console.attributes=YELLOW;
				else console.attributes=BROWN;
				console.gotoxy(posx,posy+index);
				console.putmsg(score.name,P_SAVEATR);
				console.right(20-score.name.length);
				console.putmsg(PrintPadded(score.score,11,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(PrintPadded(players.FormatDate(score.laston),8,undefined,"right"),P_SAVEATR);
				index++;
			}
		}
	}

//	GAME OBJECTS
	
	function PlayerList()
	{
		this.file=new File(gameroot + "players.ini");
		this.players=[];
		this.Init=function()
		{
			this.file.open(file_exists(this.file.name)?'r+':'w+',true);
			if(!this.file.iniGetValue(user.alias,"name"))
			{
				Log("Creating new player data: " + user.alias);
				this.file.iniSetObject(user.alias,new Player());
			}
			var plyrs=this.file.iniGetAllObjects();
			this.file.close();
			for(p in plyrs)
			{
				var plyr=plyrs[p];
				Log("Loading player data: " + plyr.name);
				if(plyr.name==user.alias) plyr.laston=time();
				this.players[plyr.name]=new Player(plyr.name,plyr.score,plyr.laston);
			}
		}
		this.FormatDate=function(timet)
		{
			var date=new Date(timet*1000);
			var m=date.getMonth()+1;
			var d=date.getDate();
			var y=date.getFullYear()-2000; //assuming no one goes back in time to 1999 to play this
			
			if(m<10) m="0"+m;
			if(d<10) d="0"+d;
			if(y<10) y="0"+y;
			
			return (m + "/" + d + "/" + y);
		}
		this.Reset=function()
		{
			file_remove(this.file.name);
			this.Init();
		}
		this.StorePlayer=function()
		{
			this.file.open('r+',true);
			this.file.iniSetObject(user.alias,this.players[user.alias]);
			this.file.close();
		}
		this.FindUser=function(alias)
		{
			return this.players[alias];
		}
		function Player(name,score,laston)
		{
			this.name=name?name:user.alias;
			this.score=score?score:0;
			this.laston=laston?laston:time();
		}
		this.Init();
	}
	function Level(grid,tiles)
	{
		this.grid=grid;
		this.tiles=tiles;
	}
	function Tile(bg,fg)
	{
		this.bg=bg;
		this.fg=fg;
		this.draw=function(selected,x,y)
		{
			Gotoxy(x,y);
			if(selected)
			{
				console.attributes=this.fg;
				console.putmsg("\xDB\xDB",P_SAVEATR);
			}
			else
			{
				console.attributes=this.bg;
				console.putmsg("  ",P_SAVEATR);
			}	
		}
	}
	
	SplashStart();
	Init();
	Lobby();
	SplashExit();
}
function Log(text)
{
	if(gamelog) gamelog.Log(text);
}

Blox();