/*
	CELLULAR AUTOMATON (Conway's Game of Life)
	for Synchronet v3.15+ (javascript)
	by Matt Johnson (2009)
	
	
	for customization or installation help contact:
	Matt Johnson ( MCMLXXIX@MDJ.ATH.CX )
*/




var gameroot;
try { barfitty.barf(barf); } catch(e) { gameroot = e.fileName; }
gameroot = gameroot.replace(/[^\/\\]*$/,"");

load("graphic.js");
load("sbbsdefs.js")
load("logging.js");
load("helpfile.js");
load("funclib.js");
 
var oldpass=console.ctrl_key_passthru;
//var gamelog=new Logger(gameroot,"cell");
var gamelog=false;

function Cells()
{
	var board;
	var clear=[];
	var generation=0;
	var colors=["\1r","\1c","\1y","\1b","\1m","\1g","\1w"];
	var highlight=["\1n","\1h"];
	var col=colors[random(7)];
	
	function Main()
	{
		while(1) 
		{
			if(ChooseConfig()) Run();
			else break;
		}
	}
	function ChooseConfig()
	{
		while(1)
		{
			console.clear();
			var list=ListConfigs();
			console.putmsg("\r\n\r\n\1nR: Random Pattern");
			console.putmsg("\r\n\1nE: Edit Pattern");
			console.putmsg("\r\n\1nESC: Exit Program");
			console.putmsg("\r\n\r\n\1nChoose a pattern: ");
			var pattern=console.getkey();
			switch(pattern.toUpperCase())
			{
				case "\x1b":
					return false;
				case "R":
					if(RandomStart()) return true;
					break;
				case "E":
					if(CustomStart()) return true;
					break;
				default:
					if(list[pattern])
					{
						LoadPattern(list[pattern]);
						return true;
					}
					break;
			}
		}
	}
	function ListConfigs()
	{
		var list=directory(gameroot+"*.bin");
		for(var f in list)
		{
			var name=file_getname(list[f]);
			console.putmsg("\r\n\1n" + f + ": " + name.substring(0,name.indexOf(".")).toUpperCase());
		}
		return list;
	}
	function LoadPattern(filename)
	{
		board=WipeBoard();
		var pattern=new Graphic(80,44);
		pattern.load(filename);
		for(var x=0;x<pattern.data.length;x++) 
		{
			for(var y=0;y<pattern.data[x].length;y++) 
			{
				if(pattern.data[x][y].ch==1)
				{
					board[x][y]=1;
				}
			}
		}
	}
	function Run()
	{
		Display();
		while(1)
		{
			var k=console.inkey(K_NOECHO|K_NOSPIN);
			switch(k)
			{
				case "\x1b":
					return;
				case " ":
					console.getkey();
					break;
				default:
					break;
			}
			mswait(5);
			board=Evolve();
			Display();
		}
	}
	function RandomStart()
	{
		board=WipeBoard(board);
		console.putmsg("\r\n\1nplace how many cells? :");
		var num=console.getnum(500);
		if(!num) return false;
		for(var n=0;n<num;n++)
		{
			var randx=random(80);
			var randy=random(44);
			if(!board[randx][randy]) board[randx][randy]=1;
			else n--;
		}
		return true;
	}
	function CustomStart()
	{
		console.clear();
		DrawLine(1,23,80);
		var cells=["\xDF","\xDC","\xDB"];
		var current=2;
		var grid=WipeBoard();
		StatusLine();
		
		console.gotoxy(40,11);
		while(1)
		{
			var k=console.inkey(K_NOECHO|K_NOSPIN);
			if(k)
			{
				var position=console.getxy();
				switch(k.toUpperCase())
				{
					case "\x1b":
						return false;
					case " ":
						Toggle();
						console.pushxy();
						StatusLine();
						console.popxy();
						break;
					case "Q":
						return true;
					case ctrl("M"):
						LoadCell(position.x,position.y);
						console.putmsg(cells[current]);
						break;
					case KEY_DOWN:
						if(position.y<22) console.down();
						else console.gotoxy(position.x,1);
						break;
					case KEY_UP:
						if(position.y>1) console.up();
						else console.gotoxy(position.x,22);
						break;
					case KEY_LEFT:
						if(position.x>1) console.left();
						else console.gotoxy(80,position.y);
						break;
					case KEY_RIGHT:
						if(position.x<80) console.right();
						else console.gotoxy(1,position.y);
						break;
					default:
						break;
				}
				board=grid;
			}
		}
		function StatusLine()
		{
			console.gotoxy(1,24);
			console.putmsg("\1c\1h[\1n\1cArrows\1h]\1n\1c Move \1h[\1n\1cEnter\1h]\1n\1c Place \1h[\1n\1cSpace\1h] \1h[\1n\1cQ\1h] Save \1h[\1n\1cEscape\1h]\1n\1c Exit \1n\1cToggle cell: \1h" + cells[current]);
		}
		function Toggle()
		{
			current++;
			if(current>2) current=0;
		}
		function LoadCell(x,y)
		{
			var t=(current==0 || current==2)?true:false;
			var b=(current==1 || current==2)?true:false;
			y=(y-1)*2;
			if(t) grid[x-1][y]=1;
			if(b) grid[x-1][y+1]=1;
		}
	}
	function WipeBoard()
	{
		var array=new Array(80);
		for(var x=0;x<array.length;x++)
		{
			array[x]=new Array(44);
		}
		return array;
	}
	function Display()
	{
		console.clear();
		var top="\xDF";
		var bottom="\xDC";
		var whole="\xDB";
		var totalcount=0;
		for(var x=0;x<board.length;x++)
		{
			var line=1;
			for(var y=0;y<board[x].length;y+=2)
			{
				var t=false;
				var b=false;
				if(board[x][y]==1) 
				{
					totalcount++;
					t=true;
				}
				if(board[x][y+1]==1) 
				{
					totalcount++;
					b=true;
				}
				if(t || b)
				{
					if(x==79 && line==24) break;
					console.gotoxy(x+1,line);
					if(!b)
					{
						c=top;
					}
					else if(!t)
					{
						c=bottom;
					}
					else c=whole;
					
					//var color=highlight[random(2)] + colors[random(8)];
					var hl=highlight[random(2)];
					console.putmsg(hl + col + c);
				}
				line++;
			}
		}
		console.gotoxy(1,24);
		console.putmsg("\1y\1h[\1n\1ySpace\1h]\1n\1y Toggle Pause \1h[\1n\1yEscape\1h]\1n\1y Quit \1h--- \1n\1yTotal Live Cells: " + totalcount + " Generation: " + generation);
	}
	function Evolve()
	{
		var nextgen=WipeBoard();
		generation++;
		for(var x=0;x<board.length;x++)
		{
			for(var y=0;y<board[x].length;y++)
			{
				if(CheckProximity(x,y))
				{
					nextgen[x][y]=1;
				}
			}
		}
		return nextgen;
	}
	function CheckProximity(x,y)
	{
		var checkx=[	0,	1,	1,	1,	0,	-1,	-1,	-1	];
		var checky=[	-1,	-1,	0,	1,	1,	1,	0,	-1	];
		var count=0;
		
		for(var i=0;i<8;i++)
		{
			var posx=x+checkx[i];
			var posy=y+checky[i];
			if(posx>=80) posx=0;
			else if(posx<0) posx=79;
			if(posy>=44) posy=0;
			else if(posy<0) posy=43;
			
			if(board[posx][posy]==1) 
			{
				count++;
			}
		}
		if(board[x][y]==1)
		{
			if(count<2 || count>3) 
			{
				return false;
			}
			else 
			{
				return true;
			}
		}
		else if(count==3)
		{
			return true;
		}
		return false;
	}
	function SplashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
	}
	function SplashExit()
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
		exit(0);
	}
	SplashStart();
	Main();
	SplashExit();
}	
function Log(txt)
{
	if(gamelog) gamelog.Log(txt);
}

Cells();