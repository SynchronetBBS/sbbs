/*
	UBER BLOX!

	A javascript block puzzle game similar to GameHouse "Super Collapse"
	by Matt Johnson (2009)

	for Synchronet v3.15+
*/

load("graphic.js");
load("sbbsdefs.js")
load("funclib.js");

var oldpass = console.ctrl_key_passthru;
var useralias = user.alias.replace(/\./g,"_");
var data = new GameData();

/* cycle client and check for updates */
function cycle() {
	client.cycle();
}
/* process updates received from the JSON database */
function processUpdates(update) {
	switch(update.operation) {
	case "UPDATE":
		var p=update.location.split(".");
		/* if we received something for a different game? */
		if(p.shift().toUpperCase() != "UBERBLOX")
			return;
		var obj=data;
		while(p.length > 1) {
			var child=p.shift();
			obj=obj[child];
		}
		obj[p.shift()] = update.data;
		data.updated=true;
		break;
	case "ERROR":
		throw(update.error_desc);
		break;
	}
}

function gotoxy(x,y)
{
	var posx=(x*3)+2;
	var posy=22-(y*2);
	console.gotoxy(posx,posy);
}
function open()
{
	client.callback=processUpdates;
	console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ";
	bbs.sys_status|=SS_MOFF;
	bbs.sys_status|=SS_PAUSEOFF;
	if(file_exists(root + "uberblox.bin")) {
		console.clear();
		var splash=new Graphic(80,22);
		splash.load(root + "uberblox.bin");
		splash.draw();
		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		console.getkey(K_NOECHO|K_NOSPIN);
	}
	console.clear();
	client.subscribe("uberblox","players");
	client.subscribe("uberblox","alltime");
}
function close()
{
	client.unsubscribe("uberblox","players");
	client.unsubscribe("uberblox","alltime");
	data.storePlayer();
	console.ctrlkey_passthru=oldpass;
	bbs.sys_status&=~SS_MOFF;
	bbs.sys_status&=~SS_PAUSEOFF;
	console.clear();
	var splash_filename=root + "exit.bin";
	if(file_exists(splash_filename)) {
		var splash=new Graphic(80,21);
		splash.load(splash_filename);
		splash.draw();

		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		console.getkey(K_NOECHO|K_NOSPIN);
	}
	console.clear();
}
function blox()
{
	const columns=20;
	const rows=11;
	const bg=[BG_GREEN,  BG_BLUE,  BG_RED,  BG_MAGENTA,  LIGHTGRAY,BG_BROWN];
	const fg=[LIGHTGREEN,LIGHTBLUE,LIGHTRED,LIGHTMAGENTA,WHITE,    YELLOW];

	var level;
	var points;
	var current;
	var gameover=false;

	const points_base=			15;
	const points_increment=		10;
	const points_bonus=			5000;
	const minimum_cluster=		3;
	const colors=				[3,3,3,4,4,4,5,5,5,6,6,6]; //COLORS PER MAP (DEFAULT ex.: levels 1-4 have 3 colors)
	const tiles_per_color=		[8,7,6,10,9,8,11,10,9,12,11,10];	//TARGET FOR LEVEL COMPLETION (DEFAULT ex. level 1 target: 220 total tiles minus 8 tiles per color times 3 colors = 196)

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
		while(1) {
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k) {
				switch(k.toUpperCase())	{
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
			cycle();
		}
	}
	function showInstructions()
	{
		console.clear(ANSI_NORMAL);
		bbs.sys_status&=~SS_PAUSEOFF;
		bbs.sys_status|=SS_PAUSEON;
		var helpfile=root + "help.doc";
		if(file_exists(helpfile)) console.printfile(helpfile);
		bbs.sys_status&=~SS_PAUSEON;
		bbs.sys_status|=SS_PAUSEOFF;
	}
	function main()
	{
		selx=0;
		sely=0;
		selection=[];
		selected=false;
		level=0;
		points=0;
		gameover=false;
		generateLevel();
		redraw();
		while(1) {
			if(gameover) {
				endGame();
				console.getkey(K_NOECHO);
				return;
			}
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k) {
				switch(k.toUpperCase()) {
				case "W":
					k = KEY_UP;
					break;
				case "S":
					k = KEY_DOWN;
					break;
				case "A":
					k = KEY_LEFT
					break;
				case "D":
					k = KEY_RIGHT;
					break;
				case " ":
					k = "\r";
					break;
				}
				switch(k.toUpperCase())	{
				case KEY_UP:
				case KEY_DOWN:
				case KEY_LEFT:
				case KEY_RIGHT:
					if(selected)
						unselect();
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
				case '?':
					showInstructions();
					redraw();
					break;
				default:
					break;
				}
			}
			cycle();
		}
	}
	function drawLobby()
	{
		lobby.draw(1,1);
		showScores();
	}
	function levelUp()
	{
		selx=0;
		sely=0;
		selection=[];
		selected=false;
		if(current.tiles == (columns * rows))
			points+=points_bonus*level;
		level+=1;
		generateLevel();
	}
	function endGame()
	{
		if(data.players[useralias].score == undefined || data.players[useralias].score < points) {
			data.players[useralias].score = points;
			data.storePlayer();
			data.allTime();
		}

		console.clear();
		gameend.draw();
		console.gotoxy(52,5);
		console.putmsg(centerString("\1n\1y" + points,13));
		console.gotoxy(52,13);
		console.putmsg(centerString("\1n\1y" + (parseInt(level)+1),13));
	}
	function init()
	{
		data.players[useralias].laston = time();
		logo=new Graphic(18,22);
		logo.load(root + "blox.bin");
		lobby=new Graphic(80,23);
		lobby.load(root + "lobby.bin");
		gameend=new Graphic(80,23);
		gameend.load(root + "gameend.bin");
	}
	function generateLevel()
	{
		var numcolors=colors[level];
		var tiles_needed=(columns*rows)-(numcolors*tiles_per_color[level]);

		grid=new Array(columns);
		for(var x=0;x<grid.length;x++) {
			grid[x]=new Array(rows);
			for(var y=0;y<grid[x].length;y++) {
				var col=random(numcolors);
				grid[x][y]=new Tile(bg[col],fg[col],x,y);
			}
		}
		current=new Level(grid,tiles_needed);
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
		if(selected) {
			removeBlocks();
			drawGrid();
			showScore();
			if(!findValidMove()) {
				if(current.tiles_remaining<=0) {
					mswait(1000);
					levelUp();
					redraw();
				}
				else {
					gameover=true;
				}
			}
			else
			{
				if(selx>=current.grid.length)
					selx=current.grid.length-1;
				if(sely>=current.grid[selx].length)
					sely=current.grid[selx].length-1;
				showPosition();
			}
		}
		else {
			counted=Grid(columns,rows);
			search(selx,sely);
			if(selection.length>=minimum_cluster) {
				select();
			}
			else {
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

		for(var s=0;s<selection.length;s++) {
			var coord=selection[s];
			current.grid[coord.x].splice(coord.y,1);
			if(current.grid[coord.x].length==0)
				current.grid.splice(coord.x,1);
		}
		current.tiles_remaining-=selection.length;
		current.tiles+=selection.length;
		if(current.tiles_remaining<0)
			current.tiles_remaining=0;
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
		console.putmsg("\1w\1h" + useralias);
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
		console.putmsg(printPadded("\1w\1h" + current.tiles_remaining,15));
	}
	function calculatePoints()
	{
		if(selection.length) {
			var p=points_base;
			for(var t=0;t<selection.length;t++) {
				p+=points_base+(t*points_increment);
			}
			return p;
		}
		return 0;
	}
	function findValidMove()
	{
		counted=Grid(columns,rows);
		for(var x=0;x<current.grid.length;x++) {
			for(var y=0;y<current.grid[x].length;y++) {
				selection=[];
				search(x,y);
				if(selection.length>=minimum_cluster) {
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
		for(var p in data.players) {
			var s=data.players[p];
			if(s.score > 0)
				sorted.push(s);
		}
		var numScores=sorted.length;
		for(n=0;n<numScores;n++) {
			for(m = 0; m < (numScores-1); m++) {
				if(sorted[m].score < sorted[m+1].score)	{
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

		var scores=sortScores().slice(0,13);

		for(var s in scores) {
			var score=scores[s];
			if(score.name==useralias)
				console.attributes=YELLOW;
			else
				console.attributes=BROWN;
			console.gotoxy(posx,posy+index);
			console.putmsg(score.name,P_SAVEATR);
			console.right(17-score.name.length);
			console.putmsg(score.sys,P_SAVEATR);
			console.right(25-score.sys.length);
			console.putmsg(printPadded(score.score,10,undefined,"right"),P_SAVEATR);
			console.right(3);
			console.putmsg(printPadded(data.formatDate(score.laston),8,undefined,"right"),P_SAVEATR);
			index++;
		}
		console.gotoxy(46,20);
		console.putmsg("\1n\1yName \1h: " + data.alltime.name);
		console.gotoxy(46,21);
		console.putmsg("\1n\1yScore\1h: " + data.alltime.score,P_SAVEATR);
	}

	init();
	mainLobby();
}

//	GAME OBJECTS
function Grid(c,r)
{
	var array=new Array(c);
	for(var cc=0;cc<array.length;cc++)
		array[cc]=new Array(r);
	return array;
}
function GameData()
{
	this.players=client.read("uberblox","players",1);
	this.alltime=client.read("uberblox","alltime",1);
	this.update=false;
	//this.month=new Date().getMonth();

	this.init=function()
	{
		// client.lock("uberblox","month",2);
		// var month = client.read("uberblox","month");
		// if(month != this.month)
			// this.reset();
		// client.unlock("uberblox","month");

		if(!this.alltime)
			this.alltime={name:"none",score:0};

		if(!this.players)
			this.players = {};
		if(!this.players[useralias]) {
			this.players[useralias] = new Player();
			client.write("uberblox","players." + useralias,this.players[useralias],2);
		}
	}
	this.formatDate=function(timet)
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
	this.reset=function()
	{
		client.write("uberblox","month",this.month);
		client.remove("uberblox","players",2);
	}
	this.storePlayer=function()
	{
		client.write("uberblox","players." + useralias,this.players[useralias],2);
	}
	this.allTime=function()
	{
		client.lock("uberblox","alltime",2);
		if(this.players[useralias].score > this.alltime.score) {
			this.alltime=client.read("uberblox","alltime");
			if(!this.alltime)
				this.alltime={name:"none",score:0};
			if(this.players[useralias].score > this.alltime.score) {
				this.alltime.score = this.players[useralias].score;
				this.alltime.name = useralias;
				client.write("uberblox","alltime",this.alltime);
			}
		}
		client.unlock("uberblox","alltime");
	}
	this.findUser=function(alias)
	{
		return this.players[alias];
	}
	this.init();
}
function Player(name,score,laston,sys)
{
	this.name=name?name:useralias;
	this.score=score?score:0;
	this.laston=laston?laston:time();
	this.sys=sys?sys:system.name;
}
function Level(grid,tiles_needed)
{
	this.grid=grid;
	this.tiles_remaining=tiles_needed;
	this.tiles=0;
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

open();
blox();
close();
