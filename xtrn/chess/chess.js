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

var stream=				new ServiceConnection("chess");
var chesschat=			new ChatEngine(chessroot,stream);
var chessplayers=		new PlayerList();
var oldpass=			console.ctrl_key_passthru;

function chess()
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

	function splashStart()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;	
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
		processGraphic();
		initMenu();
		initChat();
		redraw();
		
		notify("Please Wait...");
		getFiles();
		notice("loading player data...");
		loadPlayers();
		notice("loading game data...");
		updateTables();
		chesschat.input_line.clear();
	}
	function splashExit()
	{
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
		console.clear();
		var splash_filename=chessroot + "exit.bin";
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
		console.clear();
		exit();
	}
	function initChat()
	{
		var rows=19;
		var columns=38;
		var posx=42;
		var posy=3;
		if(!chesschat.init(room,true,columns,rows,posx,posy,false,false)) 
		{
			log("chat initialization failed");
			exit();
		}
	}
	function initMenu()
	{
		menu=new Menu("\1n","\1c\1h");
		var menu_items=[		"~Select Game"	, 
								"~New Game"		,
								"~Rankings"		,
								"~Help"			,
								"Re~draw"		];
		menu.add(menu_items);
	}
	function processGraphic()
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
	function getNotices()
	{
		chesschat.getNotices();	
	}
	function listCommands()
	{
		var list=menu.getList();
		chesschat.displayInfo(list);
	}
	function refreshCommands()
	{
		if(!tables.length) menu.disable(["S"]);
		else menu.enable(["S"]);
	}
	function getTablePointers()
	{
		var pointers=[];
		for(t in tables)
		{
			pointers.push(t);
		}
		return pointers;
	}
	function loadPlayers()
	{
		chessplayers.loadPlayers();
	}
	function updateTables()
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
					loadTable(game_files[i]);
					update=true;
				}
			}
			else 
			{
				log("loading table: " + game_files[i]);
				loadTable(game_files[i]);
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
		if(update) drawTables();
	}
	function loadTable(gamefile,index)
	{
		var table=new ChessGame(gamefile);
		if(table.gamenumber>last_table_number) last_table_number=table.gamenumber;
		tables[table.gamenumber]=table;
	}
	function drawTables()
	{
		var pointers=getTablePointers();
		var range=(pointers.length%2==1?pointers.length+1:pointers.length);
		if(tables.length>4) scrollBar.draw(scroll_index,range);

		var index=scroll_index;
		for(i in table_markers)
		{
			var posx=table_markers[i].x;
			var posy=table_markers[i].y;
			clearBlock(posx,posy,18,10);
			
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
					console.putmsg(printPadded(chessplayers.formatPlayer(tab.players[player],player,turn),19));
					console.gotoxy(x+1,y+1);
					console.putmsg(chessplayers.formatStats(tab.players[player]));
					y+=2;
				}
				table_graphic.draw(posx,posy);
				index++;
			}
		}
		write(console.ansi(BG_BLACK));
	}
	function drawLobby()
	{
		lobby_graphic.draw();
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		drawLobby();
		drawTables();
		chesschat.redraw();
	}


/*	MAIN FUNCTIONS	*/
	function main()
	{
		while(1)
		{
			//TODO: figure out why this isnt working?
			//if(!cycle()) exit();
			cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,25);
			if(k)
			{
				var range=(tables.length%2==1?tables.length+1:tables.length);
				if(clearinput) 
				{
					chesschat.input_line.clear();
					clearinput=false;
				} 
				switch(k.toUpperCase())
				{
					case KEY_LEFT:
						if(scroll_index>0 && tables.length>4) 
						{
							scroll_index-=2;
							drawTables();
						}
						break;
					case KEY_RIGHT:
						if((scroll_index+1)<=range && tables.length>4) 
						{
							scroll_index+=2;
							drawTables();
						}
						break;
					case "/":
						if(!chesschat.buffer.length) 
						{
							lobbyMenu();
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
	function cycle()
	{
		getNotices();
		updateTables();
		
		if(!stream) return -1;
		var packet=stream.receive();
		if(!packet) return 0;
		
		if(packet.room==room || packet.scope==global_scope)
		{
			switch(packet.type.toLowerCase())
			{
				case "chat":
					chesschat.processData(packet.data);
					break;
				case "chess":
					log("illegal data type received");
					break;
				default:
					log("Unknown data type received");
					break;
			}
		}
		return 1;
	}
	function lobbyMenu()
	{
		refreshCommands();
		listCommands();
		notify("\1nMake a selection: ");
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		if(menu.items[k] && menu.items[k].enabled) 
		{
			switch(k.toUpperCase())
			{
				case "N":
					startNewGame();
					break;
				case "R":
					showRankings();
					break;
				case "H":
					help();
					break;
				case "S":
					var g=selectGame();
					if(!g) break;
					GameSession(tables[g]);
					initChat();
					break;
				default:
					break;
			}
		}
		chesschat.input_line.clear();
		redraw();
	}
	function help()
	{
		//TODO: write help file
	}
	function showRankings()
	{
	}
	function selectGame()
	{
		notify("\1nEnter Table \1h#: ");
		var num=console.getkeys("\x1bQ\r",last_table_number);
		if(tables[num])
		{
			if(tables[num].password)
			{
				notify("\1nPassword: ");
				var password=console.getstr(25);
				if(password!=tables[num].password) return false;
			}
			return num;
		}
		else return false;
	}
	function startNewGame()
	{
		var newgame=new ChessGame();
		clearinput=true;
		notify("\1nRated game? [\1hY\1n,\1hN\1n]: ");
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
				notify("\1r\1hGame Creation Aborted");
				return;
		}
		notify("\1nMinimum rating? [\1hY\1n,\1hN\1n]: ");
		var minrating=console.getkeys("\x1bYN");
		switch(minrating)
		{
			case "Y":
				var maxrating=chessplayers.getPlayerRating(user.alias);
				notify("\1nEnter minimum [" + maxrating + "]: ");
				minrating=console.getkeys("\x1b",maxrating);
				newgame.minrating=(minrating?minrating:false);
				break;
			case "N":
				newgame.minrating=false;
				break;
			default:
				notify("\1r\1hGame Creation Aborted");
				return;
		}
		notify("\1nTimed game? [\1hY\1n,\1hN\1n]: ");
		var timed=console.getkeys("\x1bYN");
		switch(timed)
		{
			case "Y":
				notify("\1nTimer length? [\1h1\1n-\1h20\1n]: ");
				timed=console.getkeys("\x1b",20);
				newgame.timed=(timed?timed:false);
				break;
			case "N":
				newgame.timed=false;
				break;
			default:
				notify("\1r\1hGame Creation Aborted");
				return;
		}
		notify("\1nPrivate game? [\1hY\1n,\1hN\1n]: ");
		var password=console.getkeys("\x1bYN");
		switch(password)
		{
			case "Y":
				notify("\1nPassword: ");
				password=console.getstr(25);
				if(password.length) newgame.password=password;
				else 
				{
					notify("\1r\1hGame Creation Aborted");
					return;
				}
				break;
			case "N":
				newgame.password=false;
				break;
			default:
				notify("\1r\1hGame Creation Aborted");
				return;
		}
		notify("\1g\1hGame #" + parseInt(newgame.gamenumber,10) + " created");
		storeGame(newgame);
	}

	splashStart();
	main();
	splashExit();
}
function notify(message)
{
	chesschat.alert(message);
}
function notice(message)
{
	chesschat.notice(message);
}
function getFiles(data)
{
	notice("Synchronizing game data...");
	stream.recvfile("*.chs");
	notice("Synchronizing player data...");
	stream.recvfile("players.ini");
}
function sendFiles(filename)
{
	stream.sendfile(filename);
}
function storeGame(game)
{
	//STORE GAME DATA
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
		gFile.iniSetValue("lastmove","from",getChessPosition(game.board.lastmove.from));
		gFile.iniSetValue("lastmove","to",getChessPosition(game.board.lastmove.to));
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
				var position=getChessPosition({'x':x,'y':y});
				var section="board."+position;
				gFile.iniSetValue(section,"piece",contents.name);
				gFile.iniSetValue(section,"color",contents.color);
				if(contents.has_moved) gFile.iniSetValue(section,"hasmoved",contents.has_moved);
			}
		}
	}
	for(move in game.movelist)
	{
		gFile.iniSetValue("move." + move,"from",getChessPosition(game.movelist[move].from));
		gFile.iniSetValue("move." + move,"to",getChessPosition(game.movelist[move].to));
		gFile.iniSetValue("move." + move,"color",game.movelist[move].color);
		if(game.movelist[move].check) gFile.iniSetValue("move." + move,"check",game.movelist[move].check);
	}
	gFile.close();
	file_remove(game.gamefile);
	file_rename(gFile.name,game.gamefile);
	sendFiles(game.gamefile);
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
	this.getPlayerRating=function(name)
	{
		var fullname=this.prefix + name;
		return this.players[fullname].rating;
	}
	this.storePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(chessroot + "players.ini");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"rating",player.rating);
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.iniSetValue(id,"draws",player.draws);
		pFile.close();
		sendFiles(pFile.name);
	}
	this.formatStats=function(index)
	{
		if(!index.id) return("");
		var player=this.players[index.id];
		return(	"\1n\1yR\1h" +
						printPadded(player.rating,4,"0","right") + "\1n\1yW\1h" +
						printPadded(player.wins,3,"0","right") + "\1n\1yL\1h" +
						printPadded(player.losses,3,"0","right") + "\1n\1yS\1h" +
						printPadded(player.draws,3,"0","right"));
	}
	this.formatPlayer=function(index,color,turn)
	{
		var name=(index.id?this.players[index.id].name:"Empty Seat");
		var col=(color=="white"?"\1n\1h":"\1k\1h");
		var highlight=(color==turn?"\1r\1h":col);
		return (highlight + "[" + col + name + highlight + "]");
	}
	this.loadPlayers=function()
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
		if(update) sendFiles(pFile.name);
	}
	this.getFullName=function(name)
	{
		return(this.prefix + name);
	}
}

chess();
