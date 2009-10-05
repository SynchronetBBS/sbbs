/*
	JAVASCRIPT SEA BATTLE (Battleship)
	For Synchronet v3.15+
	Matt Johnson(2009)
*/


var gameplayers;
var gamelobby;
var gameroot;
//var gamehelp;

load("chateng.js");
load("graphic.js");
load("scrollbar.js");
//load("helpfile.js"); //TODO: write instructions!

try { barfitty.barf(barf); } catch(e) { gameroot = e.fileName; }
gameroot = gameroot.replace(/[^\/\\]*$/,"");

//var gamelog=new Logger(gameroot,"seabattl");
var gamelog=false;
var gamechat=argv[0]?argv[0]:new ChatEngine(gameroot,"seabattle",gamelog);

load(gameroot + "battle.js");
load(gameroot + "menu.js");

var oldpass=console.ctrl_key_passthru;

function GameLobby()
{
	var table_graphic=new Graphic(10,5);
	table_graphic.load(gameroot+"bsmall.bin");
	var lobby_graphic=new Graphic(80,24);
	lobby_graphic.load(gameroot+"lobby.bin");
	var clearinput=true;
	var scrollBar=new Scrollbar(3,24,35,"horizontal","\1y");
	var scroll_index=0;
	var last_table_number=0;
	var table_markers=[];
	var tables=[];
	var menu;

	function SplashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear(ANSI_NORMAL);
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
		ProcessGraphic();
		InitChat();
		InitMenu();
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
	function InitChat()
	{
		var rows=19;
		var columns=38;
		var posx=42;
		var posy=3;
		var input_line={x:42,y:23,columns:38};
		gamechat.Init("Sea-Battle Lobby",input_line,columns,rows,posx,posy,false,false,"\1y");
	}
	function InitMenu()
	{
		menu=new Menu(	gamechat.input_line.x,gamechat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Select Game"					, 
								"~New Game"						,
								"~Rankings"						,
								"~Help"							,
								"Re~draw"						];
		menu.add(menu_items);
	}
	function ProcessGraphic()
	{
		for(posx=0;posx<lobby_graphic.data.length;posx++) 
		{
			for(posy=0;posy<lobby_graphic.data[posx].length;posy++)
			{
				var position={"x" : posx+1, "y" : posy+1};
				if(lobby_graphic.data[posx][posy].ch=="@") 
				{
					table_markers.push(position);
					lobby_graphic.data[posx][posy].ch=" ";
				}
			}
		}
	}
	function ListCommands()
	{
		var list=menu.getList();
		gamechat.DisplayInfo(list);
	}
	function RefreshCommands()
	{
		if(!tables.length) menu.disable(["S"]);
		else menu.enable(["S"]);
	}
	function GetTablePointers()
	{
		var pointers=[];
		for(t in tables)
		{
			pointers.push(t);
		}
		return pointers;
	}
	function UpdateTables()
	{
		var update=false;
		var game_files=directory(gameroot+"*.bom");
		for(i in game_files)
		{
			var filename=file_getname(game_files[i]);
			var gamenumber=parseInt(filename.substring(0,filename.indexOf(".")),10);
			if(tables[gamenumber]) 
			{
				var lastupdated=file_date(game_files[i]);
				var lastloaded=tables[gamenumber].lastupdated;
				if(lastupdated>lastloaded)
				{
					Log("Updating existing game: " +  game_files[i]);
					LoadTable(game_files[i]);
					update=true;
				}
			}
			else 
			{
				Log("Loading new table: " + game_files[i]);
				LoadTable(game_files[i]);
				update=true;
			}
		}
		for(t in tables)
		{
			var table=tables[t];
			if(table && !file_exists(table.gamefile.name)) 
			{
				Log("Removing deleted game: " + table.gamefile.name);
				delete tables[t];
				update=true;
			}
		}
		return update;
	}
	function LoadTable(gamefile,index)
	{
		var table=new GameData(gamefile);
		if(table.gamenumber>last_table_number) last_table_number=table.gamenumber;
		tables[table.gamenumber]=table;
	}
	function DrawTables()
	{
		var pointers=GetTablePointers();
		var range=(pointers.length%2==1?pointers.length+1:pointers.length);
		if(tables.length>4) scrollBar.draw(scroll_index,range);

		var index=scroll_index;
		for(i in table_markers)
		{
			var posx=table_markers[i].x;
			var posy=table_markers[i].y;
			ClearBlock(posx,posy,19,10);
			
			var pointer=pointers[index];
			if(tables[pointer])
			{
				var tab=tables[pointer];
				
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
				table_graphic.draw(posx,posy);
				index++;
			}
		}
	}
	function DrawLobby()
	{
		lobby_graphic.draw();
		write(console.ansi(BG_BLACK));
	}
	function Redraw()
	{
		console.clear(ANSI_NORMAL);
		UpdateTables();
		DrawLobby();
		DrawTables();
		gamechat.Redraw();
	}


/*	MAIN FUNCTIONS	*/
	function Main()
	{
		while(1)
		{
			if(UpdateTables()) DrawTables();
			gamechat.Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				var range=(tables.length%2==1?tables.length+1:tables.length);
				if(clearinput) 
				{
					gamechat.ClearInputLine();
					clearinput=false;
				} 
				switch(k.toUpperCase())
				{
					case KEY_LEFT:
						if(scroll_index>0 && tables.length>4) 
						{
							scroll_index-=2;
							DrawTables();
						}
						break;
					case KEY_RIGHT:
						if((scroll_index+1)<=range && tables.length>4) 
						{
							scroll_index+=2;
							DrawTables();
						}
						break;
					case "/":
						if(!gamechat.buffer.length) 
						{
							RefreshCommands();
							ListCommands();
							Commands();
							Redraw();
						}
						else if(!Chat(k,gamechat)) return;
						break;
					case "\x1b":	
						SplashExit();
						break;
					default:
						if(!Chat(k,gamechat)) return;
						break;
				}
			}
		}
	}
	function Commands()
	{
		menu.displayHorizontal();
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		if(menu.items[k] && menu.items[k].enabled) 
			switch(k.toUpperCase())
			{
				case "N":
					if(StartNewGame()) InitChat();
					break;
				case "R":
					ShowRankings();
					break;
				case "H":
					gamehelp.help("lobby");
					break;
				case "S":
					if(SelectGame()) InitChat();
					break;
				default:
					break;
			}
		gamechat.ClearInputLine();
	}
	function Help()
	{
		//TODO: write help file
	}
	function ShowRankings()
	{
	}
	function SelectGame()
	{
		gamechat.Alert("\1nEnter Table \1h#: ");
		var num=console.getkeys("\x1bQ\r",last_table_number);
		if(tables[num])
		{
			if(tables[num].password)
			{
				gamechat.Alert("\1nPassword: ");
				var password=console.getstr(25);
				if(password!=tables[num].password) return false;
			}
			GameSession(tables[num]);
			return true;
		}
		else return false;
	}
	function StartNewGame()
	{
		var newgame=new GameData();
		clearinput=true;

		gamechat.Alert("\1nSingle Player Game? [\1hy\1n,\1hN\1n]: ");
		var single=console.getkeys("\x1bYN");
		switch(single)
		{
			case "Y":
				newgame.singleplayer=true;
				break;
			case "\x1b":
				gamechat.Alert("\1r\1hGame Creation Aborted");
				return false;
			default:
				newgame.singleplayer=false;
				break;
		}
		
		if(!newgame.singleplayer)
		{
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
					return false;
				default:
					newgame.password=false;
					break;
			}
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
				return false;
			default:
				newgame.multishot=false;
				break;
		}
		
		gamechat.Alert("\1nBonus attack for each hit? [\1hy\1n,\1hN\1n]: ");
		var bonus=console.getkeys("\x1bYN");
		switch(bonus)
		{
			case "Y":
				newgame.bonusattack=true;
				break;
			case "\x1b":
				gamechat.Alert("\1r\1hGame Creation Aborted");
				return false;
			default:
				newgame.bonusattack=false;
				break;
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
				return false;
			default:
				newgame.spectate=false;
				break;
		}
		var newsession=new GameSession(newgame,true);
		return true;
	}
	SplashStart();
	Redraw();
	Main();
	SplashExit();
}
function PlayerList()
{
	this.prefix=system.qwk_id + "."; //BBS prefix FOR INTERBBS PLAY?? WE'LL SEE
	this.players=[];
	//this.current;
	
	this.StorePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(gameroot + "players.dat");
		pFile.open("r+",true); 
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.close();
	}
	this.FormatStats=function(player)
	{
		if(this.GetAlias(player)=="CPU") return "";
		var player=this.players[player];
		return(			"\1n\1cW\1h" +
						PrintPadded(player.wins,3,"0","right") + "\1n\1cL\1h" +
						PrintPadded(player.losses,3,"0","right"));
	}
	this.FormatPlayer=function(player,turn)
	{
		var name=this.GetAlias(player);
		var highlight=(player==turn?"\1r\1h":"\1n\1h");
		return (highlight + "[" + "\1n" + name + highlight + "]");
	}
	this.LoadPlayers=function()
	{
		var pFile=new File(gameroot + "players.dat");
		pFile.open((file_exists(pFile.name) ? "r+":"w+"),true); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name"))
		{
			pFile.iniSetObject(this.prefix + user.alias,new GamePlayer(user.alias));
		}
		var players=pFile.iniGetSections();
		for(var player in players)
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
	function GamePlayer(name)
	{
		Log("Creating new player: " + name);
		this.name=name;
		this.wins=0;
		this.losses=0;
	}
	this.LoadPlayers();
}
function Log(text)
{
	if(gamelog) gamelog.Log(text);
}

//gamehelp=new HelpFile(gameroot + "seabattl.hlp");
gameplayers=new PlayerList();

GameLobby();