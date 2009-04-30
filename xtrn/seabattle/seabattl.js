/*
	JAVASCRIPT SEA BATTLE (Battleship)
	For Synchronet v3.15+
	Matt Johnson(2009)
*/


var gameplayers;
var gamelobby;
var gameroot;
var gamechat;
load("chateng.js");
load("graphic.js");
load("scrollbar.js");

try { barfitty.barf(barf); } catch(e) { gameroot = e.fileName; }
gameroot = gameroot.replace(/[^\/\\]*$/,"");

//var gamelog=new Logger(gameroot,"seabattl");
var gamelog=false;

gamechat=argv[0]?argv[0]:new ChatEngine(gameroot,"seabattle",gamelog);

load(gameroot + "battle.js");
load(gameroot + "menu.js");

var oldpass=console.ctrl_key_passthru;

function GameLobby()
{
	this.table_graphic=new Graphic(10,5);
	this.table_graphic.load(gameroot+"bsmall.bin");
	this.lobby_graphic=new Graphic(80,24);
	this.lobby_graphic.load(gameroot+"lobby.bin");
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
	this.SplashExit=function()
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
		exit(0);
	}
	this.InitChat=function()
	{
		var rows=19;
		var columns=38;
		var posx=42;
		var posy=3;
		var input_line={x:42,y:23,columns:38};
		gamechat.Init("Sea Battle Lobby",input_line,columns,rows,posx,posy,false,"\1y");
		this.Redraw();
	}
	this.InitMenu=function()
	{
		this.menu=new Menu(	gamechat.input_line.x,gamechat.input_line.y,"\1n","\1c\1h");
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
		gamechat.DisplayInfo(list);
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
		var game_files=directory(gameroot+"*.bom");
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
		var table=new GameData(gamefile);
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
			ClearBlock(posx,posy,19,10);
			
			var pointer=pointers[index];
			if(this.tables[pointer])
			{
				var tab=this.tables[pointer];
				
				console.gotoxy(posx+11,posy);
				console.putmsg("\1n\1yTABLE: \1h" + tab.gamenumber);
				
				console.gotoxy(posx+11,posy+1);
				var spec=(tab.spectate?"Y":"N");
				console.putmsg("\1n\1ySPECT: \1h" + spec);
				
				console.gotoxy(posx+11,posy+2);
				var multi=(tab.multishot?"Y":"N");
				console.putmsg("\1n\1yMULTI: \1h" + multi);

				console.gotoxy(posx+11,posy+3);
				var bonus=(tab.bonusattack?"Y":"N");
				console.putmsg("\1n\1yBONUS: \1h" + bonus);

				if(tab.password)
				{
					console.gotoxy(posx+1,posy+4);
					console.putmsg("\1r\1hPRIVATE");
				}
				var turn=false;
				var x=posx;
				var y=posy+6;
				if(tab.started && !tab.finished) turn=tab.turn;
				for(player in tab.players)
				{
					console.gotoxy(x,y);
					console.putmsg(PrintPadded(gameplayers.FormatPlayer(tab.players[player].id,turn),19));
					console.gotoxy(x+1,y+1);
					console.putmsg(gameplayers.FormatStats(tab.players[player].id));
					y+=2;
				}
				this.table_graphic.draw(posx,posy);
				index++;
			}
		}
	}
	this.DrawLobby=function()
	{
		this.lobby_graphic.draw();
		this.DrawTables();
		write(console.ansi(BG_BLACK));
	}
	this.Redraw=function()
	{
		write(console.ansi(ANSI_NORMAL));
		console.clear();
		this.DrawLobby();
		gamechat.Redraw();
	}


/*	MAIN FUNCTIONS	*/
	this.Main=function()
	{
		while(1)
		{
			this.UpdateTables();
			gamechat.Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				var range=(this.tables.length%2==1?this.tables.length+1:this.tables.length);
				if(this.clearinput) 
				{
					gamechat.ClearInputLine();
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
						if(!gamechat.buffer.length) 
						{
							this.RefreshCommands();
							this.ListCommands();
							this.LobbyMenu();
							this.Redraw();
						}
						else if(!Chat(k,gamechat)) return;
						break;
					case "\x1b":	
						this.SplashExit();
						break;
					default:
						if(!Chat(k,gamechat)) return;
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
		gamechat.ClearInputLine();
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
		gamechat.Alert("\1nEnter Table \1h#: ");
		var num=console.getkeys("\x1bQ\r",this.last_table_number);
		if(this.tables[num])
		{
			if(this.tables[num].password)
			{
				gamechat.Alert("\1nPassword: ");
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
		var newgame=new GameData();
		this.clearinput=true;

		gamechat.Alert("\1nPrivate game? [\1hy\1n,\1hN\1n]: ");
		var password=console.getkeys("\x1bYN");
		switch(password)
		{
			case "Y":
				gamechat.Alert("\1nPassword: ");
				password=console.getstr(25);
				if(password.length) newgame.password=password;
				else 
				{
					gamechat.Alert("\1r\1hGame Creation Aborted");
					return;
				}
				break;
			case "\x1b":
				gamechat.Alert("\1r\1hGame Creation Aborted");
				return;
			default:
				newgame.password=false;
				break;
		}

		gamechat.Alert("\1nAttacks equal to # of ships? [\1hy\1n,\1hN\1n]: ");
		var multi=console.getkeys("\x1bYN");
		switch(multi)
		{
			case "Y":
				newgame.multishot=true;
				break;
			case "\x1b":
				gamechat.Alert("\1r\1hGame Creation Aborted");
				return;
			default:
				newgame.multishot=false;
				break;
		}
		
		if(!newgame.multishot)
		{
			gamechat.Alert("\1nBonus attack for each hit? [\1hy\1n,\1hN\1n]: ");
			var bonus=console.getkeys("\x1bYN");
			switch(bonus)
			{
				case "Y":
					newgame.bonusattack=true;
					break;
				case "\x1b":
					gamechat.Alert("\1r\1hGame Creation Aborted");
					return;
				default:
					newgame.bonusattack=false;
					break;
			}
		}
		
		gamechat.Alert("\1nAllow spectators to see board? [\1hy\1n,\1hN\1n]: ");
		var spectate=console.getkeys("\x1bYN");
		switch(spectate)
		{
			case "Y":
				newgame.spectate=true;
				break;
			case "\x1b":
				gamechat.Alert("\1r\1hGame Creation Aborted");
				return;
			default:
				newgame.spectate=false;
				break;
		}
		var newsession=new GameSession(newgame,true);
	}
	this.SplashStart();
	this.Main();
	this.SplashExit();
}
function GamePlayer(name)
{
	Log("Creating new player: " + name);
	this.name=name;
	this.wins=0;
	this.losses=0;
}
function PlayerList()
{
	this.prefix=system.qwk_id + "."; //BBS prefix FOR INTERBBS PLAY?? WE'LL SEE
	this.players=[];
	this.current;
	
	this.StorePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(gameroot + "players.dat");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.close();
	}
	this.FormatStats=function(player)
	{
		var player=this.players[player];
		return(			"\1n\1cW\1h" +
						PrintPadded(player.wins,3,"0","right") + "\1n\1cL\1h" +
						PrintPadded(player.losses,3,"0","right"));
	}
	this.FormatPlayer=function(player,turn)
	{
		var name=this.players[player].name;
		var highlight=(player==turn?"\1r\1h":"\1n\1h");
		return (highlight + "[" + "\1n" + name + highlight + "]");
	}
	this.LoadPlayers=function()
	{
		var pFile=new File(gameroot + "players.dat");
		pFile.open(file_exists(pFile.name) ? "r+":"w+"); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name"))
		{
			pFile.iniSetObject(this.prefix + user.alias,new GamePlayer(user.alias));
		}
		var players=pFile.iniGetSections();
		for(player in players)
		{
			this.players[players[player]]=pFile.iniGetObject(players[player]);
			Log("Loaded player: " + this.players[players[player]].name);
		}
		pFile.close();
	}
	this.GetAlias=function(fullname)
	{
		return(fullname.substr(fullname.indexOf(".")+1));
	}
	this.GetFullName=function(alias)
	{
		return(this.prefix + alias);
	}
	this.LoadPlayers();
}
function Log(text)
{
	if(gamelog) gamelog.Log(text);
}

gameplayers=new PlayerList();
gamelobby=new GameLobby();