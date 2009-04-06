/*
	JAVASCRIPT CHESS 
	For Synchronet v3.15+
	Matt Johnson(2008)
*/


var chessplayers;
var chesslobby;
var chessroot;
var chesschat;
load("chateng.js");
load("graphic.js");
load("scrollbar.js");

try { barfitty.barf(barf); } catch(e) { chessroot = e.fileName; }
chessroot = chessroot.replace(/[^\/\\]*$/,"");
var chesslog=new Logger(chessroot,"chess");

chesschat=argv[0]?argv[0]:new ChatEngine(chessroot,"chess",chesslog);

load(chessroot + "chessbrd.js");
load(chessroot + "menu.js");

var oldpass=console.ctrl_key_passthru;

function ChessLobby()
{
	this.table_graphic=new Graphic(8,4);
	this.table_graphic.load(chessroot+"chessbrd.bin");
	this.lobby_graphic=new Graphic(40,24);
	this.lobby_graphic.load(chessroot+"lobby.bin");
	this.clearinput=true;
	this.table_markers=[];
	this.scrollBar=new Scrollbar(1,1,1,24,"\1y");
	this.list_index=0;
	this.tables;
	this.table_index;

/**	GAME INITIALIZATION FUNCTIONS	**/
	this.SplashStart=function()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
		this.ProcessGraphic();
		this.Init();
	}
	this.SplashExit=function(ERR)
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		if(ERR)
			switch(ERR)
			{
				case 100:
					Log("Error: No chessroot directory specified");
					break;
				default:
					Log("Error: Unknown");
					break;
			}
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		write(console.ansi(ANSI_NORMAL));
		exit(0);
	}
	this.Init=function()
	{
		var rows=21;
		var columns=37;
		var posx=42;
		var posy=2;
		var input_line={x:42,y:23,columns:37};
		chesschat.Init("Chess Lobby",input_line,columns,rows,posx,posy,true);
		this.Redraw();
	}
	this.ProcessGraphic=function()
	{
		for(posy=0;posy<24;posy++)
		{
			for(posx=0;posx<this.lobby_graphic.data.length;posx++) 
			{
				var position={"x" : posx+1, "y" : posy+1};
				if(this.lobby_graphic.data[posx][posy].ch=="@") 
				{
					this.table_markers.push(position);
					this.lobby_graphic.data[posx][posy].ch=" ";
				}
			}
		}
	}
	this.UpdateTables=function()
	{
		//TODO: ?? MAYBE WRITE A MORE EFFICIENT WAY OF UPDATING THE LOBBY TABLE LIST?
	}
	this.LoadTables=function()
	{
		var game_files=directory(chessroot+"*.chs");
		this.tables=[];
		this.table_index=[];
		for(i in game_files)
		{
			var table=new ChessGame(game_files[i]);
			this.table_index[table.gamenumber]=this.tables.length;
			this.tables.push(table);
		}
	}
	this.DrawTables=function()
	{
		if(this.tables.length>4) this.scrollBar.draw(this.list_index,this.tables.length);
		var index=this.list_index;
		for(i in this.table_markers)
		{
			if(this.tables[index])
			{
				posx=this.table_markers[i].x;
				posy=this.table_markers[i].y;
				
				var tab=this.tables[index];
				console.gotoxy(posx+9,posy);
				console.putmsg("\1n\1yTABLE:\1h" + tab.gamenumber);
				console.gotoxy(posx+9,posy+2);
				console.putmsg("\1n\1yRATED: \1h" + (tab.rated?"Y":"N"));
				console.gotoxy(posx+9,posy+3);
				console.putmsg("\1n\1yTIMED: \1h" + (tab.timed?tab.timed:"N"));
				
				this.FormatStats(tab.players["white"].player,posx,posy+5,"white");
				this.FormatStats(tab.players["black"].player,posx,posy+7,"black");
				this.table_graphic.draw(posx,posy);
				index++;
			}
			else break;
		}
	}
	this.FormatStats=function(player,x,y,color)
	{
		var col=(color=="white"?"\1n\1h":"\1k\1h");
		console.gotoxy(x,y);
		
		if(!player) console.putmsg(col+"[EMPTY SEAT]");
		else
		{
			console.putmsg(col + player.name);
			console.gotoxy(x,y+1);
			console.putmsg(	"\1y\1hR\1n\1y" +
							PrintPadded(player.rating,4,"0") + " \1hW\1n\1y" +
							PrintPadded(player.wins,3,"0") + "\1hL\1n\1y" +
							PrintPadded(player.losses,3,"0") + "\1hS\1n\1y" +
							PrintPadded(player.draws,3,"0"));
		}
	}
	this.DrawLobby=function()
	{
		this.lobby_graphic.draw();
		this.DrawTables();
	}


/**	MAIN FUNCTIONS	**/
	this.Main=function()
	{
		while(1)
		{
			chesschat.Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				if(this.clearinput) 
				{
					chesschat.ClearInputLine();
					this.clearinput=false;
				} 
				switch(k.toUpperCase())
				{
					case KEY_UP:
						if(this.list_index>0) this.list_index-=2;
						break;
					case KEY_DOWN:
						if(this.list_index<this.tables.length) this.list_index+=2;
						break;
					case "/":
						this.LobbyMenu();
						break;
					case "\x1b":	
						this.SplashExit();
						break;
					case "?":
						this.Help();
						break;
					default:
						if(Chat(chesschat,k)) break; 
						else this.SplashExit();
				}
				this.LoadTables();
				this.DrawLobby();
			}
		}
	}
	this.Help=function()
	{
		//TODO: write help file
	}
	this.ShowRankings=function()
	{
	}
	this.SelectGame=function()
	{
		chesschat.Alert("\1nEnter Table \1h#: ");
		var num=console.getkeys("\x1bQ\r",this.tables.length);
		var tnum=this.table_index[num];
		if(this.tables[tnum])
		{
			var play=new GameSession(this.tables[tnum]);
			return true;
		}
		else return false;
	}
	this.Redraw=function()
	{
		write(console.ansi(ANSI_NORMAL));
		console.clear();
		this.LoadTables();
		this.DrawLobby();
		chesschat.Redraw();
	}
	this.StartNewGame=function()
	{
		var newgame=new ChessGame();
		this.clearinput=true;

		chesschat.Alert("\1nChoose a side [\1hB\1n,\1hW\1n]: ");
		var color=console.getkeys("\x1bWB");
		switch(color)
		{
			case "W":
				newgame.players["white"].player=chessplayers.current;
				break;
			case "B":
				newgame.players["black"].player=chessplayers.current;
				break;
			default:
				chesschat.Alert("\1r\1hGame Creation Aborted");
				return;
		}
		chesschat.Alert("\1nRated game? [\1hY\1n,\1hN\1n]: ");
		var rated=console.getkeys("\x1bYN");
		switch(rated)
		{
			case "Y":
				newgame.rated=true;
				break;
			case "N":
				newgame.rated=false;
				break;
			default:
				chesschat.Alert("\1r\1hGame Creation Aborted");
				return;
		}
		chesschat.Alert("\1nTimed game? [\1hY\1n,\1hN\1n]: ");
		var timed=console.getkeys("\x1bYN");
		switch(timed)
		{
			case "Y":
				chesschat.Alert("\1nTimer length? [\1h1\1n-\1h20\1n]: ")
				timed=console.getkeys("\x1b",20);
				newgame.timed=(timed?timed:false);
				break;
			case "N":
				newgame.timed=false;
				break;
			default:
				chesschat.Alert("\1r\1hGame Creation Aborted");
				return;
		}
		chesschat.Alert("\1g\1hGame #" + parseInt(newgame.gamenumber,10) + " created");
		newgame.StoreGame();
	}
	this.LobbyMenu=function()
	{
		chesschat.Alert("\1c\1hS\1nelect Game Re\1c\1hd\1nraw \1c\1hN\1new Game \1c\1hR\1nankings");
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO);
		switch(k.toUpperCase())
		{
			case "N":
				this.StartNewGame();
				break;
			case "R":
				this.ShowRankings();
				break;
			case "D":
				this.Redraw();
				break;
			case "S":
				if(this.SelectGame()) this.Init();
				break;
			default:
				break;
		}
		chesschat.ClearInputLine();
	}

	this.SplashStart();
	this.Main();
	this.SplashExit();
}
function PlayerList()
{
	this.players;
	this.current;
	this.player_index=[];
	this.LoadPlayers=function()
	{
		var pFile=new File(chessroot + "players.dat");
		pFile.open(file_exists(pFile.name) ? "r+":"w+"); 
		
		if(!pFile.iniGetValue(user.alias,"name"))
		{
			pFile.iniSetObject(user.alias,new Player(user.alias));
		}
		this.players=pFile.iniGetAllObjects();
		for(player in this.players)
		{
			this.player_index[this.players[player].name]=player;
			Log("Loading player: " + this.players[player].name);
		}
		pFile.close();
	}
	function Player(name)
	{
		Log("Creating new player: " + name);
		this.name=name;
		this.rating=1200;
		this.wins=0;
		this.losses=0;
		this.draws=0;
	}
	this.LoadPlayers();
	this.current=this.players[this.player_index[user.alias]];
}
function Log(text)
{
	chesslog.Log(text);
}

chessplayers=new PlayerList();
chesslobby=new ChessLobby();