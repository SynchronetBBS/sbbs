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
	this.lobby_graphic=new Graphic(80,24);
	this.lobby_graphic.load(chessroot+"lobby.bin");
	this.clearinput=true;
	this.scrollBar=new Scrollbar(3,24,35,"horizontal","\1y");
	this.scroll_index=0;
	this.last_table_number=0;
	this.table_markers=[];
	this.tables=[];
	this.menu;

	this.SplashStart=function()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
		this.ProcessGraphic();
		this.InitChat();
		this.InitMenu();
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
	this.InitChat=function()
	{
		var rows=19;
		var columns=38;
		var posx=42;
		var posy=3;
		var input_line={x:42,y:23,columns:38};
		chesschat.Init("Chess Lobby",input_line,columns,rows,posx,posy,false,"\1y");
		this.Redraw();
	}
	this.InitMenu=function()
	{
		this.menu=new Menu(		chesschat.input_line.x,chesschat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Select Game"					, 
								"~New Game"						,
								"~Rankings"						,
								"~Help"							,
								"Re~draw"						];
		this.menu.add(menu_items);
	}
	this.ProcessGraphic=function()
	{
		for(posx=0;posx<this.lobby_graphic.data.length;posx++) 
		{
			for(posy=0;posy<this.lobby_graphic.data[posx].length;posy++)
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
	this.ListCommands=function()
	{
		var list=this.menu.getList();
		chesschat.DisplayInfo(list);
	}
	this.RefreshCommands=function()
	{
		if(!this.tables.length) this.menu.disable(["S"]);
		else this.menu.enable(["S"]);
	}
	this.GetTablePointers=function()
	{
		var pointers=[];
		for(t in this.tables)
		{
			pointers.push(t);
		}
		return pointers;
	}
	this.UpdateTables=function()
	{
		var update=false;
		var game_files=directory(chessroot+"*.chs");
		for(i in game_files)
		{
			var filename=file_getname(game_files[i]);
			var gamenumber=parseInt(filename.substring(0,filename.indexOf(".")));
			if(this.tables[gamenumber]) 
			{
				var lastupdated=file_date(game_files[i]);
				var lastloaded=this.tables[gamenumber].lastupdated;
				if(lastupdated>lastloaded)
				{
					Log("Updating existing game: " +  game_files[i]);
					this.LoadTable(game_files[i]);
					update=true;
				}
			}
			else 
			{
				Log("Loading new table: " + game_files[i]);
				this.LoadTable(game_files[i]);
				update=true;
			}
		}
		for(t in this.tables)
		{
			var table=this.tables[t];
			if(table && !file_exists(table.gamefile.name)) 
			{
				Log("Removing deleted game: " + table.gamefile.name);
				delete this.tables[t];
				update=true;
			}
		}
		if(update) this.DrawTables();
	}
	this.LoadTable=function(gamefile,index)
	{
		var table=new ChessGame(gamefile);
		if(table.gamenumber>this.last_table_number) this.last_table_number=table.gamenumber;
		this.tables[table.gamenumber]=table;
	}
	this.DrawTables=function()
	{
		var pointers=this.GetTablePointers();
		var range=(pointers.length%2==1?pointers.length+1:pointers.length);
		if(this.tables.length>4) this.scrollBar.draw(this.scroll_index,range);

		var index=this.scroll_index;
		for(i in this.table_markers)
		{
			var posx=this.table_markers[i].x;
			var posy=this.table_markers[i].y;
			ClearBlock(posx,posy,18,10);
			
			var pointer=pointers[index];
			if(this.tables[pointer])
			{
				var tab=this.tables[pointer];
				console.gotoxy(posx+9,posy);
				console.putmsg("\1n\1yTABLE: \1h" + tab.gamenumber);
				console.gotoxy(posx+9,posy+1);
				console.putmsg("\1n\1yRATED: \1h" + (tab.rated?"Y":"N"));
				console.gotoxy(posx+9,posy+2);
				console.putmsg("\1n\1yTIMED: \1h" + (tab.timed?tab.timed:"N"));
				if(tab.minrating)
				{
					console.gotoxy(posx+9,posy+3);
					console.putmsg("\1n\1yMIN: \1h" + tab.minrating + "+");
				}
				if(tab.password)
				{
					console.gotoxy(posx,posy+4);
					console.putmsg("\1r\1hPRIVATE");
				}
				
				var turn=false;
				var x=posx;
				var y=posy+6;
				if(tab.started && !tab.finished) turn=tab.turn;
				for(player in tab.players)
				{
					console.gotoxy(x,y);
					console.putmsg(PrintPadded(chessplayers.FormatPlayer(tab.players[player],player,turn),19));
					console.gotoxy(x+1,y+1);
					console.putmsg(chessplayers.FormatStats(tab.players[player]));
					y+=2;
				}
				this.table_graphic.draw(posx,posy);
				index++;
			}
		}
		write(console.ansi(BG_BLACK));
	}
	this.DrawLobby=function()
	{
		this.lobby_graphic.draw();
		this.DrawTables();
	}
	this.Redraw=function()
	{
		write(console.ansi(ANSI_NORMAL));
		console.clear();
		this.DrawLobby();
		chesschat.Redraw();
	}


/*	MAIN FUNCTIONS	*/
	this.Main=function()
	{
		while(1)
		{
			this.UpdateTables();
			chesschat.Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				var range=(this.tables.length%2==1?this.tables.length+1:this.tables.length);
				if(this.clearinput) 
				{
					chesschat.ClearInputLine();
					this.clearinput=false;
				} 
				switch(k.toUpperCase())
				{
					case KEY_LEFT:
						if(this.scroll_index>0 && this.tables.length>4) 
						{
							this.scroll_index-=2;
							this.DrawTables();
						}
						break;
					case KEY_RIGHT:
						if((this.scroll_index+1)<=range && this.tables.length>4) 
						{
							this.scroll_index+=2;
							this.DrawTables();
						}
						break;
					case "/":
						if(!chesschat.buffer.length) 
						{
							this.RefreshCommands();
							this.ListCommands();
							this.LobbyMenu();
							this.Redraw();
						}
						else if(!Chat(k,chesschat)) return;
						break;
					case "\x1b":	
						this.SplashExit();
						break;
					default:
						if(!Chat(k,chesschat)) return;
						break;
				}
			}
		}
	}
	this.LobbyMenu=function()
	{
		this.menu.displayHorizontal();
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		if(this.menu.items[k] && this.menu.items[k].enabled) 
			switch(k.toUpperCase())
			{
				case "N":
					this.StartNewGame();
					break;
				case "R":
					this.ShowRankings();
					break;
				case "H":
					this.Help();
					break;
				case "D":
					this.Redraw();
					break;
				case "S":
					if(this.SelectGame()) this.InitChat();
					break;
				default:
					break;
			}
		chesschat.ClearInputLine();
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
		var num=console.getkeys("\x1bQ\r",this.last_table_number);
		if(this.tables[num])
		{
			if(this.tables[num].password)
			{
				chesschat.Alert("\1nPassword: ");
				var password=console.getstr(25);
				if(password!=this.tables[num].password) return false;
			}
			var play=new GameSession(this.tables[num]);
			return true;
		}
		else return false;
	}
	this.StartNewGame=function()
	{
		var newgame=new ChessGame();
		this.clearinput=true;
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
		chesschat.Alert("\1nMinimum rating? [\1hY\1n,\1hN\1n]: ");
		var minrating=console.getkeys("\x1bYN");
		switch(minrating)
		{
			case "Y":
				var maxrating=chessplayers.GetPlayerRating(user.alias);
				chesschat.Alert("\1nEnter minimum [" + maxrating + "]: ");
				minrating=console.getkeys("\x1b",maxrating);
				newgame.minrating=(minrating?minrating:false);
				break;
			case "N":
				newgame.minrating=false;
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
				chesschat.Alert("\1nTimer length? [\1h1\1n-\1h20\1n]: ");
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
		chesschat.Alert("\1nPrivate game? [\1hY\1n,\1hN\1n]: ");
		var password=console.getkeys("\x1bYN");
		switch(password)
		{
			case "Y":
				chesschat.Alert("\1nPassword: ");
				password=console.getstr(25);
				if(password.length) newgame.password=password;
				else 
				{
					chesschat.Alert("\1r\1hGame Creation Aborted");
					return;
				}
				break;
			case "N":
				newgame.password=false;
				break;
			default:
				chesschat.Alert("\1r\1hGame Creation Aborted");
				return;
		}
		chesschat.Alert("\1g\1hGame #" + parseInt(newgame.gamenumber,10) + " created");
		newgame.StoreGame();
	}
	this.SplashStart();
	this.Main();
	this.SplashExit();
}
function ChessPlayer(name)
{
	Log("Creating new player: " + name);
	this.name=name;
	this.rating=1200;
	this.wins=0;
	this.losses=0;
	this.draws=0;
}
function PlayerList()
{
	this.prefix=system.qwk_id + "."; //BBS prefix FOR INTERBBS PLAY?? WE'LL SEE
	this.players=[];
	this.current;
	
	this.GetPlayerRating=function(name)
	{
		var fullname=this.prefix + name;
		return this.players[fullname].rating;
	}
	this.StorePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(chessroot + "players.dat");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"rating",player.rating);
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.iniSetValue(id,"draws",player.draws);
		pFile.close();
	}
	this.FormatStats=function(index)
	{
		if(!index.id) return("");
		var player=this.players[index.id];
		return(	"\1n\1yR\1h" +
						PrintPadded(player.rating,4,"0","right") + "\1n\1yW\1h" +
						PrintPadded(player.wins,3,"0","right") + "\1n\1yL\1h" +
						PrintPadded(player.losses,3,"0","right") + "\1n\1yS\1h" +
						PrintPadded(player.draws,3,"0","right"));
	}
	this.FormatPlayer=function(index,color,turn)
	{
		var name=(index.id?this.players[index.id].name:"Empty Seat");
		var col=(color=="white"?"\1n\1h":"\1k\1h");
		var highlight=(color==turn?"\1r\1h":col);
		return (highlight + "[" + col + name + highlight + "]");
	}
	this.LoadPlayers=function()
	{
		var pFile=new File(chessroot + "players.dat");
		pFile.open(file_exists(pFile.name) ? "r+":"w+"); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name"))
		{
			pFile.iniSetObject(this.prefix + user.alias,new ChessPlayer(user.alias));
		}
		var players=pFile.iniGetSections();
		for(player in players)
		{
			this.players[players[player]]=pFile.iniGetObject(players[player]);
			Log("Loaded player: " + this.players[players[player]].name);
		}
		pFile.close();
	}
	this.GetFullName=function(name)
	{
		return(this.prefix + name);
	}
	this.LoadPlayers();
}
function Log(text)
{
	chesslog.Log(text);
}

chessplayers=new PlayerList();
chesslobby=new ChessLobby();