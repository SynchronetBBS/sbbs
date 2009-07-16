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
	
	function Main()
	{
		var list=ListConfigs();
		var config=ChooseConfig(list);
		if(!config) exit(0);
		
		Run();
	}
	function ChooseConfig(list)
	{
		console.putmsg("\r\n\1nChoose a pattern: ");
		while(1)
		{
			var pattern=console.inkey();
			switch(pattern.toUpperCase())
			{
				case "\x1b":
					return false;
				case "R":
					RandomStart();
					return true;
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
			console.putmsg("\r\n\1n" + f + ":" + list[f]);
		}
		console.crlf();
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
			var k=console.inkey();
			switch(k)
			{
				case "\x1b":
					return;
				default:
					break;
			}
			mswait(25);
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
					console.putmsg(c);
				}
				line++;
			}
		}
		console.gotoxy(1,24);
		console.putmsg("\1nTotal: " + totalcount + " Generation: " + generation);
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
}	
function Log(txt)
{
	if(gamelog) gamelog.Log(txt);
}

Cells();