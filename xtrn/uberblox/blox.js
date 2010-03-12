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
load("commclient.js");
 
var oldpass=console.ctrl_key_passthru;
var interbbs=argv[0];
var stream=interbbs?new ServiceConnection("uberblox"):false;

function blox()
{
	const columns=20;
	const rows=11;
	const bg=[BG_GREEN,BG_BLUE,BG_RED,BG_CYAN,BG_BROWN,BG_MAGENTA];
	const fg=[LIGHTGREEN,LIGHTBLUE,LIGHTRED,LIGHTCYAN,YELLOW,LIGHTMAGENTA];

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

	function mainLobby()
	{
		drawLobby();
		while(1)
		{
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				switch(k.toUpperCase())
				{
					case "\x1b":	
					case "Q":
						return;
					case "I":
						showInstructions();
						drawLobby();
						break;
					case "P":
						main();
						drawLobby();
						break;
					case "R":
						drawLobby();
						break;
					default:
						break;
				}
			}
		}
	}
	function showInstructions()
	{
		console.clear(ANSI_NORMAL);
		bbs.sys_status&=~SS_PAUSEOFF;
		bbs.sys_status|=SS_PAUSEON;	
		var helpfile=gameroot + "help.doc";
		if(file_exists(helpfile)) console.printfile(helpfile);
		bbs.sys_status&=~SS_PAUSEON;
		bbs.sys_status|=SS_PAUSEOFF;	
	}
	function main()
	{
		level=0;
		points=0;
		gameover=false;
		generateLevel();
		redraw();
		while(1)
		{
			if(gameover) 
			{
				endGame();
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
							unselect();
						}
						movePosition(k);
						break;
					case "R":
						redraw();
						break;
					case "\x1b":	
					case "Q":
						gameover=true;
						break;
					case "\r":
					case "\n":
						processSelection();
						break;
					default:
						break;
				}
			}
		}
	}
	function drawLobby()
	{
		lobby.draw(1,1);
		showScores();
	}
	function levelUp()
	{
		level+=1;
		generateLevel();
	}
	function endGame()
	{
		if(players.players[user.alias].score<points) 
		{
			players.players[user.alias].score=points;
			players.StorePlayer();
		}
		console.clear();
		gameend.draw();
		console.gotoxy(52,5);
		console.putmsg(centerString("\1n\1y" + points,13));
		console.gotoxy(52,13);
		console.putmsg(centerString("\1n\1y" + (parseInt(level)+1),13));
	}
	function splashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status|=SS_PAUSEOFF;
		console.clear();
	}
	function splashExit()
	{
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		players.storePlayer();
		if(interbbs) sendFiles("players.ini");
		console.clear(ANSI_NORMAL);
		var splash_filename=gameroot + "exit.bin";
		if(!file_exists(splash_filename)) exit();
		
		var splash_size=file_size(splash_filename);
		splash_size/=2;		
		splash_size/=80;	
		var splash=new Graphic(80,splash_size);
		splash.load(splash_filename);
		splash.draw();
		
		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		while(console.inkey(K_NOECHO|K_NOSPIN)==="");
		console.clear(ANSI_NORMAL);
		exit();
	}
	function init()
	{
		if(interbbs) getFiles("players.ini");
		logo=new Graphic(18,22);
		logo.load(gameroot + "blox.bin");
		lobby=new Graphic(80,23);
		lobby.load(gameroot + "lobby.bin");
		gameend=new Graphic(80,23);
		gameend.load(gameroot + "gameend.bin");
		players=new PlayerList();
	}
	function generateLevel()
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
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		drawGrid();
		drawLogo();
		drawInfo();
		showPosition();
	}
	function drawGrid()
	{
		clearBlock(1,1,60,23);
		for(var x=0;x<current.grid.length;x++)
		{
			for(var y=0;y<current.grid[x].length;y++)
			{
				current.grid[x][y].draw(false,x,y);
			}
		}
		console.attributes=ANSI_NORMAL;
	}
	function drawLogo()
	{
		logo.draw(62,1);
		console.attributes=ANSI_NORMAL;
	}
	function drawInfo()
	{
		showPlayer();
		showScore();
		showSelection(selection.length);
		showLevel();
		showTiles();
		console.gotoxy(1,24);
		console.putmsg("\1nArrow keys : move \1k\1h[\1nENTER\1k\1h]\1n : select \1k\1h[\1nQ\1k\1h]\1n : quit \1k\1h[\1n?\1k\1h]\1n : help \1k\1h[\1nR\1k\1h]\1n : redraw");
	}
	function gotoxy(x,y)
	{
		var posx=(x*3)+2;
		var posy=22-(y*2);
		console.gotoxy(posx,posy);
	}
	function movePosition(k)
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
		showPosition()
	}
	function select()
	{
		for(var s in selection)
		{
			var spot=selection[s];
			var tile=current.grid[spot.x][spot.y];
			tile.draw(true,spot.x,spot.y);
		}
		selected=true;
		showSelection(selection.length);
	}
	function unselect()
	{
		for(var s in selection)
		{
			var spot=selection[s];
			var tile=current.grid[spot.x][spot.y];
			tile.draw(false,spot.x,spot.y);
		}
		selection=[];
		selected=false;
		showSelection(0);
	}
	function processSelection()
	{
		if(selected) 
		{
			removeBlocks();
			drawGrid();
			showScore();
			if(!findValidMove())
			{
				if(current.tiles<=0) 
				{
					mswait(1000);
					levelUp();
					redraw();
				}
				else gameover=true;
			}
			else
			{
				if(selx>=current.grid.length) selx=current.grid.length-1;
				if(sely>=current.grid[selx].length) sely=current.grid[selx].length-1;
				showPosition();
			}
		}
		else
		{
			counted=Grid(columns,rows);
			search(selx,sely);
			if(selection.length>=minimum_cluster)			
			{
				select();
			}
			else
			{
				selection=[];
			}
		}
	}
	function search(x,y)
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
					search(spot.x,spot.y);
				}
			}
		}
	}
	function Coord(x,y)
	{
		this.x=x;
		this.y=y;
	}
	function sortSelection()
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
	function removeBlocks()
	{
		sortSelection();
		
		for(var s=0;s<selection.length;s++)
		{
			var coord=selection[s];
			current.grid[coord.x].splice(coord.y,1);
			if(current.grid[coord.x].length==0) current.grid.splice(coord.x,1);
		}
		current.tiles-=selection.length;
		if(current.tiles<0) current.tiles=0;
		points+=calculatePoints();
		selection=[];
		unselect();
		showTiles();
		
	}
	function showPosition()
	{
		current.grid[selx][sely].draw(true,selx,sely);
	}
	function showPlayer()
	{
		console.gotoxy(63,9);
		console.putmsg("\1w\1h" + user.alias);
	}
	function showScore()
	{
		console.gotoxy(63,12);
		console.putmsg(printPadded("\1w\1h" + points,15));
	}
	function showSelection()
	{
		console.gotoxy(63,15);
		console.putmsg(printPadded("\1w\1h" + calculatePoints(),15));
	}
	function showLevel()
	{
		console.gotoxy(63,18);
		console.putmsg("\1w\1h" + (parseInt(level)+1));
	}
	function showTiles()
	{
		console.gotoxy(63,21);
		console.putmsg(printPadded("\1w\1h" + current.tiles,15));
	}
	function calculatePoints()
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
	function findValidMove()
	{
		counted=Grid(columns,rows);
		for(var x=0;x<current.grid.length;x++)
		{
			for(var y=0;y<current.grid[x].length;y++)
			{
				selection=[];
				search(x,y); 
				if(selection.length>=minimum_cluster) 
				{
					selection=[];
					return true;
				}
			}
		}
		return false;
	}
	function sortScores()
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
	function showScores()
	{
		var posx=3;
		var posy=4;
		var index=0;
		
		var scores=sortScores();
		for(var s in scores)
		{
			var score=scores[s];
			if(score.score>0)
			{
				if(score.name==user.alias) console.attributes=YELLOW;
				else console.attributes=BROWN;
				console.gotoxy(posx,posy+index);
				console.putmsg(score.name,P_SAVEATR);
				console.right(17-score.name.length);
				console.putmsg(score.sys,P_SAVEATR);
				console.right(25-score.sys.length);
				console.putmsg(printPadded(score.score,10,undefined,"right"),P_SAVEATR);
				console.right(3);
				console.putmsg(printPadded(players.FormatDate(score.laston),8,undefined,"right"),P_SAVEATR);
				index++;
			}
		}
	}

//	GAME OBJECTS
	
	function Grid(c,r)
	{
		var array=new Array(c);
		for(var cc=0;cc<array.length;cc++)
		{
			array[cc]=new Array(r);
		}
		return array;
	}
	function PlayerList()
	{
		this.file=new File(gameroot + "players.ini");
		this.players=[];
		this.init=function()
		{
			this.file.open(file_exists(this.file.name)?'r+':'w+',true);
			if(!this.file.iniGetValue(user.alias,"name"))
			{
				this.file.iniSetObject(user.alias,new Player());
			}
			var plyrs=this.file.iniGetAllObjects();
			this.file.close();
			for(p in plyrs)
			{
				var plyr=plyrs[p];
				if(plyr.name==user.alias) plyr.laston=time();
				this.players[plyr.name]=new Player(plyr.name,plyr.score,plyr.laston,plyr.sys);
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
			this.init();
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
		this.init();
	}
	function Player(name,score,laston,sys)
	{
		this.name=name?name:user.alias;
		this.score=score?score:0;
		this.laston=laston?laston:time();
		this.sys=sys?sys:system.name;
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
			gotoxy(x,y);
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
	
	splashStart();
	init();
	mainLobby();
	splashExit();
}
function getFiles(mask)
{
	stream.recvfile(mask);
}
function sendFiles(mask)
{
	stream.sendfile(mask);
}


blox();