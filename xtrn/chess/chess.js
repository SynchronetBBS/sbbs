/*
	JAVASCRIPT CHESS 2.0 INTERBBS
	For Synchronet v3.15+
	Matt Johnson(2009)
*/

load("chateng.js");
load("graphic.js");
load("sbbsdefs.js");

//load("helpfile.js"); //TODO: write instructions
var chessroot=js.exec_dir;
if(!chessroot)
{
	try { barfitty.barf(barf); } catch(e) { chessroot = e.fileName; }
	chessroot = chessroot.replace(/[^\/\\]*$/,"");
}
load(chessroot + "chessbrd.js");
load(chessroot + "menu.js");

var stream=			new GameConnection("chess");
var chesschat=			new ChatEngine(chessroot,stream);
var chessplayers=		new PlayerList();
var oldpass=			console.ctrl_key_passthru;

function ChessLobby()
{
	var table_graphic=new Graphic(8,4);
	table_graphic.load(chessroot+"chessbrd.bin");
	var lobby_graphic=new Graphic(80,24);
	lobby_graphic.load(chessroot+"lobby.bin");
	var clearinput=true;
	var scrollBar=new Scrollbar(3,24,35,"horizontal","\1y");
	var scroll_index=0;
	var last_table_number=0;
	var table_markers=[];
	var tables=[];
	var menu;
	var room="Chess Lobby";

	function SplashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
		ProcessGraphic();
		InitMenu();
		InitChat();
		Redraw();
		
		Alert("Please Wait...");
		GetFiles();
		Notice("Loading player data...");
		LoadPlayers();
		Notice("Loading game data...");
		UpdateTables();
		chesschat.ClearInputLine();
	}
	function SplashExit()
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		stream.close();
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=(ANSI_NORMAL);
		exit(0);
	}
	function InitChat()
	{
		var rows=19;
		var columns=38;
		var posx=42;
		var posy=3;
		chesschat.Init(room,true,columns,rows,posx,posy,false,false,"\1y");
	}
	function InitMenu()
	{
		menu=new Menu("\1n","\1c\1h");
		var menu_items=[		"~Select Game"	, 
								"~New Game"		,
								"~Rankings"		,
								"~Help"			,
								"Re~draw"		];
		menu.add(menu_items);
	}
	function ProcessGraphic()
	{
		for(var posx=0;posx<lobby_graphic.data.length;posx++) 
		{
			for(var posy=0;posy<lobby_graphic.data[posx].length;posy++)
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
	function GetNotices()
	{
		chesschat.GetNotices();	
	}
	function ListCommands()
	{
		var list=menu.getList();
		chesschat.DisplayInfo(list);
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
	function LoadPlayers()
	{
		chessplayers.LoadPlayers();
	}
	function UpdateTables()
	{
		var update=false;
		var game_files=directory(chessroot+"*.chs");
		for(var i=0;i<game_files.length;i++)
		{
			var filename=file_getname(game_files[i]);
			var gamenumber=parseInt(filename.substring(0,filename.indexOf(".")),10);
			if(tables[gamenumber]) 
			{
				var lastupdated=file_date(game_files[i]);
				var lastloaded=tables[gamenumber].lastupdated;
				if(lastupdated>lastloaded)
				{
					log("Updating game: " +  game_files[i]);
					LoadTable(game_files[i]);
					update=true;
				}
			}
			else 
			{
				log("Loading table: " + game_files[i]);
				LoadTable(game_files[i]);
				update=true;
			}
		}
		for(t in tables)
		{
			var table=tables[t];
			if(table && !file_exists(table.gamefile)) 
			{
				log("Removing deleted game: " + table.gamefile);
				delete tables[t];
				update=true;
			}
		}
		if(update) DrawTables();
	}
	function LoadTable(gamefile,index)
	{
		var table=new ChessGame(gamefile);
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
			ClearBlock(posx,posy,18,10);
			
			var pointer=pointers[index];
			if(tables[pointer])
			{
				var tab=tables[pointer];
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
				table_graphic.draw(posx,posy);
				index++;
			}
		}
		write(console.ansi(BG_BLACK));
	}
	function DrawLobby()
	{
		lobby_graphic.draw();
	}
	function Redraw()
	{
		console.clear(ANSI_NORMAL);
		DrawLobby();
		DrawTables();
		chesschat.Redraw();
	}


/*	MAIN FUNCTIONS	*/
	function Main()
	{
		while(1)
		{
			Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,25);
			if(k)
			{
				var range=(tables.length%2==1?tables.length+1:tables.length);
				if(clearinput) 
				{
					chesschat.ClearInputLine();
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
						if(!chesschat.buffer.length) 
						{
							LobbyMenu();
						}
						else if(!Chat(k,chesschat)) return;
						break;
					case "\x1b":	
						return;
					default:
						if(!Chat(k,chesschat)) return;
						break;
				}
			}
		}
	}
	function Cycle()
	{
		GetNotices();
		UpdateTables();
		
		if(!stream) return;
		var packet=stream.receive();
		if(!packet) return;
		
		if(packet.room==room || packet.scope==global_scope)
		{
			switch(packet.type.toLowerCase())
			{
				case "chat":
					chesschat.ProcessData(packet.data);
					break;
				case "chess":
					log("illegal data type received");
					break;
				default:
					log("Unknown data type received");
					break;
			}
		}
	}
	function LobbyMenu()
	{
		RefreshCommands();
		ListCommands();
		chesschat.Alert("\1nMake a selection: ");
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		if(menu.items[k] && menu.items[k].enabled) 
		{
			switch(k.toUpperCase())
			{
				case "N":
					StartNewGame();
					break;
				case "R":
					ShowRankings();
					break;
				case "H":
					Help();
					break;
				case "S":
					var g=SelectGame();
					if(!g) break;
					GameSession(tables[g]);
					InitChat();
					break;
				default:
					break;
			}
		}
		chesschat.ClearInputLine();
		Redraw();
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
		chesschat.Alert("\1nEnter Table \1h#: ");
		var num=console.getkeys("\x1bQ\r",last_table_number);
		if(tables[num])
		{
			if(tables[num].password)
			{
				chesschat.Alert("\1nPassword: ");
				var password=console.getstr(25);
				if(password!=tables[num].password) return false;
			}
			return num;
		}
		else return false;
	}
	function StartNewGame()
	{
		var newgame=new ChessGame();
		clearinput=true;
		Alert("\1nRated game? [\1hY\1n,\1hN\1n]: ");
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
				Alert("\1r\1hGame Creation Aborted");
				return;
		}
		Alert("\1nMinimum rating? [\1hY\1n,\1hN\1n]: ");
		var minrating=console.getkeys("\x1bYN");
		switch(minrating)
		{
			case "Y":
				var maxrating=chessplayers.GetPlayerRating(user.alias);
				Alert("\1nEnter minimum [" + maxrating + "]: ");
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
		Alert("\1nTimed game? [\1hY\1n,\1hN\1n]: ");
		var timed=console.getkeys("\x1bYN");
		switch(timed)
		{
			case "Y":
				Alert("\1nTimer length? [\1h1\1n-\1h20\1n]: ");
				timed=console.getkeys("\x1b",20);
				newgame.timed=(timed?timed:false);
				break;
			case "N":
				newgame.timed=false;
				break;
			default:
				Alert("\1r\1hGame Creation Aborted");
				return;
		}
		Alert("\1nPrivate game? [\1hY\1n,\1hN\1n]: ");
		var password=console.getkeys("\x1bYN");
		switch(password)
		{
			case "Y":
				Alert("\1nPassword: ");
				password=console.getstr(25);
				if(password.length) newgame.password=password;
				else 
				{
					Alert("\1r\1hGame Creation Aborted");
					return;
				}
				break;
			case "N":
				newgame.password=false;
				break;
			default:
				Alert("\1r\1hGame Creation Aborted");
				return;
		}
		Alert("\1g\1hGame #" + parseInt(newgame.gamenumber,10) + " created");
		StoreGame(newgame);
	}

	SplashStart();
	Main();
	SplashExit();
}
function ChessPlayer(name)
{
	log("Creating new player: " + name);
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
	this.GetPlayerRating=function(name)
	{
		var fullname=this.prefix + name;
		return this.players[fullname].rating;
	}
	this.StorePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(chessroot + "players.ini");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"rating",player.rating);
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.iniSetValue(id,"draws",player.draws);
		pFile.close();
		SendFiles(pFile.name);
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
		var update=false;
		var pFile=new File(chessroot + "players.ini");
		pFile.open(file_exists(pFile.name) ? "r+":"w+"); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name"))
		{
			pFile.iniSetObject(this.prefix + user.alias,new ChessPlayer(user.alias));
			update=true;
		}
		var players=pFile.iniGetSections();
		for(player in players)
		{
			this.players[players[player]]=pFile.iniGetObject(players[player]);
		}
		pFile.close();
		if(update) SendFiles(pFile.name);
	}
	this.GetFullName=function(name)
	{
		return(this.prefix + name);
	}
}

function Alert(message)
{
	chesschat.Alert(message);
}
function Notice(message)
{
	chesschat.Notice(message);
}
function GetFiles(data)
{
	Notice("Synchronizing game data...");
	stream.recvfile("*.chs");
	Notice("Synchronizing player data...");
	stream.recvfile("players.ini");
}
function SendFiles(filename)
{
	stream.sendfile(filename);
}
function StoreGame(game)
{
	//STORE GAME DATA
	log("Storing game " + game.gamenumber);
	var gFile=new File(game.gamefile+".tmp");
	gFile.open("w+");
	
	var wplayer;
	var bplayer;
	for(player in game.players)
	{
		gFile.iniSetValue(null,player,game.players[player].id);
	}
	gFile.iniSetValue(null,"gamenumber",game.gamenumber);
	gFile.iniSetValue(null,"turn",game.turn);

	if(game.started) gFile.iniSetValue(null,"started",game.started);
	if(game.finished) gFile.iniSetValue(null,"finished",game.finished);
	if(game.rated) gFile.iniSetValue(null,"rated",game.rated);
	if(game.timed) gFile.iniSetValue(null,"timed",game.timed);
	if(game.password) gFile.iniSetValue(null,"password",game.password);
	if(game.minrating) gFile.iniSetValue(null,"minrating",game.minrating);
	if(game.board.lastmove) 
	{
		gFile.iniSetValue("lastmove","from",GetChessPosition(game.board.lastmove.from));
		gFile.iniSetValue("lastmove","to",GetChessPosition(game.board.lastmove.to));
	}
	if(game.draw)
	{
		gFile.iniSetValue(null,"draw",game.draw);
	}
	for(x in game.board.grid)
	{
		for(y in game.board.grid[x])
		{
			var contents=game.board.grid[x][y].contents;
			if(contents)
			{
				var position=GetChessPosition({'x':x,'y':y});
				var section="board."+position;
				gFile.iniSetValue(section,"piece",contents.name);
				gFile.iniSetValue(section,"color",contents.color);
				if(contents.has_moved) gFile.iniSetValue(section,"hasmoved",contents.has_moved);
			}
		}
	}
	for(move in game.movelist)
	{
		gFile.iniSetValue("move." + move,"from",GetChessPosition(game.movelist[move].from));
		gFile.iniSetValue("move." + move,"to",GetChessPosition(game.movelist[move].to));
		gFile.iniSetValue("move." + move,"color",game.movelist[move].color);
		if(game.movelist[move].check) gFile.iniSetValue("move." + move,"check",game.movelist[move].check);
	}
	gFile.close();
	file_remove(game.gamefile);
	file_rename(gFile.name,game.gamefile);
	SendFiles(game.gamefile);
}

ChessLobby();
