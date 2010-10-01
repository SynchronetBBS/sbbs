/*
	JAVASCRIPT CHESS 2.0 INTERBBS
	For Synchronet v3.15+
	Matt Johnson(2009)
*/

load("chateng.js");
load("graphic.js");
load("sbbsdefs.js");
load("funclib.js");

//load("helpfile.js"); //TODO: write instructions
var root=js.exec_dir;
load(root + "chessbrd.js");
load(root + "menu.js");

var stream=			argv[0];
var chat=				new ChatEngine(root);
var chessplayers=		new PlayerList();
var oldpass=			console.ctrl_key_passthru;

function lobby()
{
	var table_graphic=new Graphic(8,4);
	table_graphic.load(root+"chessbrd.bin");
	var lobby_graphic=new Graphic(80,24);
	lobby_graphic.load(root+"lobby.bin");
	var clearinput=true;
	var scrollBar=new Scrollbar(3,24,35,"horizontal","\1y");
	var scroll_index=0;
	var last_table_number=0;
	var table_markers=[];
	var tables=[];
	var menu;

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
		
		notice("loading player data...");
		loadPlayers();
		notice("loading game data...");
		updateTables();
		chat.input_line.clear();
	}
	function splashExit()
	{
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
		console.clear();
		var splash_filename=root + "exit.bin";
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
		chat.init(38,19,42,3);
		chat.input_line.init(42,23,38,"","\1n");
		chat.joinChan("chess lobby",user.alias,user.name);
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
	function listCommands()
	{
		var list=menu.getList();
		chat.chatroom.clear();
		console.gotoxy(chat.chatroom);
		console.pushxy();
		for(l=0;l<list.length;l++) {
			console.putmsg(list[l]);
			console.popxy();
			console.down();
			console.pushxy();
		}
	}
	function menuPrompt(msg)
	{
		console.popxy();
		console.down();
		console.pushxy();
		console.putmsg(msg);
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
		var game_files=directory(root+"*.chs");
		for(var i=0;i<game_files.length;i++)
		{
			var filename=file_getname(game_files[i]);
			var gameNumber=parseInt(filename.substring(0,filename.indexOf(".")),10);
			if(tables[gameNumber]) 
			{
				var lastupdated=file_date(game_files[i]);
				var lastloaded=tables[gameNumber].lastupdated;
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
		if(table.gameNumber>last_table_number) last_table_number=table.gameNumber;
		tables[table.gameNumber]=table;
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
				console.putmsg("\1n\1yTABLE: \1h" + tab.gameNumber);
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
		chat.redraw();
	}


/*	MAIN FUNCTIONS	*/
	function main()
	{
		while(1) {
			cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,25);
			if(k) {
				var range=(tables.length%2==1?tables.length+1:tables.length);
				if(clearinput) {
					chat.input_line.clear();
					clearinput=false;
				} 
				switch(k.toUpperCase())
				{
					case KEY_LEFT:
						if(scroll_index>0 && tables.length>4) {
							scroll_index-=2;
							drawTables();
						}
						break;
					case KEY_RIGHT:
						if((scroll_index+1)<=range && tables.length>4) {
							scroll_index+=2;
							drawTables();
						}
						break;
					case "/":
						if(!chat.input_line.buffer.length) {
							lobbyMenu();
						}
						else if(!Chat(k,chat)) return;
						break;
					case "\x1b":	
						return;
					default:
						if(!Chat(k,chat)) return;
						break;
				}
			}
		}
	}
	function cycle()
	{
		updateTables();
		chat.cycle();
	}
	function lobbyMenu()
	{
		refreshCommands();
		listCommands();
		menuPrompt("\1nMake a selection: ");
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_UPPER);
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
					playGame(tables[g]);
					initChat();
					break;
				default:
					break;
			}
		}
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
		menuPrompt("\1nEnter Table \1h#: ");
		var num=console.getkeys("\x1bQ\r",last_table_number);
		if(tables[num])
		{
			if(tables[num].password)
			{
				menuPrompt("\1nPassword: ");
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
		menuPrompt("\1nRated game? [\1hY\1n,\1hN\1n]: ");
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
				notice("\1r\1hGame Creation Aborted");
				return;
		}
		menuPrompt("\1nMinimum rating? [\1hY\1n,\1hN\1n]: ");
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
				notice("\1r\1hGame Creation Aborted");
				return;
		}
		menuPrompt("\1nTimed game? [\1hY\1n,\1hN\1n]: ");
		var timed=console.getkeys("\x1bYN");
		switch(timed)
		{
			case "Y":
				menuPrompt("\1nTimer length? [\1h1\1n-\1h20\1n]: ");
				timed=console.getkeys("\x1b",20);
				newgame.timed=(timed?timed:false);
				break;
			case "N":
				newgame.timed=false;
				break;
			default:
				notice("\1r\1hGame Creation Aborted");
				return;
		}
		menuPrompt("\1nPrivate game? [\1hY\1n,\1hN\1n]: ");
		var password=console.getkeys("\x1bYN");
		switch(password)
		{
			case "Y":
				menuPrompt("\1nPassword: ");
				password=console.getstr(25);
				if(password.length) newgame.password=password;
				else 
				{
					notice("\1r\1hGame Creation Aborted");
					return;
				}
				break;
			case "N":
				newgame.password=false;
				break;
			default:
				notice("\1r\1hGame Creation Aborted");
				return;
		}
		notice("\1g\1hGame #" + parseInt(newgame.gameNumber,10) + " created");
		storeGame(newgame);
	}

	splashStart();
	main();
	splashExit();
}
function playGame(game) 
{
	var room;
	var menu;
	var currentplayer;
	var infobar=true; //TOGGLE FOR GAME INFORMATION BAR AT TOP OF CHAT WINDOW (default: true)
	
/*	
	CHAT ENGINE DEPENDENT FUNCTIONS
	
	some of these functions are redundant
	so as to make modification of the 
	method of data transfer between nodes/systems
	simpler
*/
	function initGame()
	{
		loadGameData();
		room="chess table " + game.gameNumber;
		
		currentplayer=game.currentplayer;
		game.board.side=(currentplayer?currentplayer:"white");
	}
	function initChat()
	{
		chat.init(38,15,42,8);
		chat.input_line.init(42,24,38,"","\1n");
		chat.partChan("chess lobby",user.alias);
		chat.joinChan("chess table " + game.gameNumber,user.alias,user.name);
	}
	function initMenu()
	{
		menu=new Menu(			"\1n","\1c\1h");
		var menu_items=[		"~Sit"							, 
								"~Resign"						,
								"~Offer Draw"					,
								"~New Game"						,
								"~Move"							,
								"Re~verse"						,
								"Toggle Board E~xpansion"		,
								"~Help"							,
								"Move ~List"					,
								"Re~draw"						];
		menu.add(menu_items);
		refreshCommands();
	}
	function main()
	{
		redraw();
		scanCheck();
		if(game.turn==game.currentplayer) {
			notice("\1g\1hIt's your turn.");
			notice("\1n\1gUse arrow keys or number pad to move around, and [\1henter\1n\1g] to select.");
			notice("\1n\1gPress [\1hescape\1n\1g] to cancel.");
		}
		while(1) {
			cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k) {
				switch(k)
				{
				case "/":
					if(!chat.input_line.buffer.length) {
						showMenu();
					}
					else if(!Chat(k,chat) && handleExit()) {
						chat.partChan("chess table " + game.gameNumber,user.alias);
						return;
					}
					break;
				case "\x1b":	
					if(handleExit()) {
						chat.partChan("chess table " + game.gameNumber,user.alias);
						return true;
					}
					break;
				case "1":
				case "2":
				case "3":
				case "4":
				case "6":
				case "7":
				case "8":
				case "9":
					if(chat.input_line.buffer.length) {
						if(!Chat(k,chat) && handleExit()) {					
							chat.partChan("chess table " + game.gameNumber,user.alias);
							return;
						}
					}
				case KEY_UP:
				case KEY_DOWN:
				case KEY_LEFT:
				case KEY_RIGHT:
					if(game.turn==game.currentplayer) {
						tryMove();
						game.board.drawLastMove();
						break;
					} 
				default:
					if(!Chat(k,chat) && handleExit()) {					
						chat.partChan("chess table " + game.gameNumber,user.alias);
						return;
					}
					break;
				}
			}
		}
	}
	function handleExit()
	{
		if(game.finished)
		{
			if(file_exists(game.gamefile.name))
				file_remove(game.gamefile.name);
		}
		else if(game.timed && game.started && game.movelist.length)
		{
			notify("\1c\1hForfeit this game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
			if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
			endGame("loss");
		}
		return true;
	}
	function refreshCommands()
	{
		if(console.screen_rows<50) menu.disable(["X"]);
		if(game.started || game.finished || currentplayer) menu.disable(["S"]);
		else if(game.password && game.password != user.alias) menu.disable(["S"]);
		else if(game.minrating && game.minrating > chessplayers.getPlayerRating(user.alias)) menu.disable(["S"]);
		else menu.enable(["S"]);
		if(!game.finished || !currentplayer) menu.disable(["N"]);
		else menu.enable(["N"]);
		if(!game.started || game.turn!=currentplayer || game.finished) 
		{
			menu.disable(["M"]);
			menu.disable(["R"]);
		}
		else 
		{
			menu.enable(["M"]);
			menu.enable(["R"]);
		}
		if(game.draw || game.turn != currentplayer) menu.disable(["O"]);
		else
		{
			menu.enable(["O"]);
		}
		if(!game.movelist.length) menu.disable(["L"]);
		else menu.enable(["L"]);
	}
	function showMenu()
	{	
		refreshCommands();
		listCommands();
		menuPrompt("Make a selection:");
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		clearAlert();
		if(k) {
			chat.chatroom.clear();
			chat.redraw();
			if(menu.items[k] && menu.items[k].enabled) {
				switch(k)
				{
					case "R":
						resign();
						break;
					case "S":
						joinGame();
						break;
					case "H":
						help();
						break;
					case "D":
						redraw();
						break;
					case "O":
						offerDraw();
						break;
					case "M":
						tryMove();
						game.board.drawLastMove();
						break;
					case "V":
						reverse();
						redraw();
						break;
					case "L":
						listMoves();
						break;
					case "X":
						toggleBoardSize();
						break;
					case "N":
						newGame();
						break;
					default:
						break;
				}
				chat.chatroom.clear();
				chat.redraw();
			}
			else log("Invalid or Disabled key pressed: " + k);
		}
	}
	function toggleBoardSize()
	{
		game.board.large=game.board.large?false:true;
		if(game.board.large)
		{
			infobar=false;
			chat.init(room,77,3,2,45);
			chat.input_line.init(2,50,77,"","\1n");
			//reinitialize chat beneath large board
		}
		else
		{
			initChat();
			//reinitialize chat beside small board
		}
		initMenu();
		redraw();
	}
	function help()
	{
	}
	function listCommands()
	{
		if(game.board.large) return;
		var list=menu.getList();
		chat.chatroom.clear();
		console.gotoxy(chat.chatroom);
		console.pushxy();
		for(l=0;l<list.length;l++) {
			console.putmsg(list[l]);
			console.popxy();
			console.down();
			console.pushxy();
		}
	}
	function menuPrompt(msg)
	{
		console.popxy();
		console.down();
		console.pushxy();
		console.putmsg(msg);
	}
	function infoBar()
	{
		console.gotoxy(chat.chatroom.x,1);
		console.putmsg("\1k\1h[\1cescape\1k\1h]\1n return to lobby \1k\1h[\1c/\1k\1h]\1n menu");
		if(infobar)	{
			var x=chat.chatroom.x;
			var y=3;
			var turn=false;
			if(game.started && !game.finished) turn=game.turn;
			
			for(player in game.players) {
				console.gotoxy(x,y);
				console.putmsg(printPadded(chessplayers.formatPlayer(game.players[player],player,turn),21));
				console.putmsg(chessplayers.formatStats(game.players[player]));
				if(game.timed && game.started) {
					console.gotoxy(x,y+1);
					game.players[player].timer.display(game.turn);
				}
				y+=2;
			}
		}
	}
	function showTimers()
	{
		if(!infobar) return;
		var x=chat.x;
		var y=4;
		for(player in game.players)
		{
			var timer=game.players[player].timer;
			var current=time();
			if(current!=timer.lastshown && game.turn==player)
			{
				console.gotoxy(x,y);
				game.players[player].timer.display(game.turn);
			}
			y+=2;
		}
	}
	function clearAlert()
	{
		if(!chat.input_line.buffer.length) chat.input_line.clear();
	}
	function notice(txt)
	{
		chat.chatroom.notice(txt);
	}
	function notify(msg)
	{
		chat.chatroom.alert(msg);
	}
	function cycle()
	{
		chat.cycle();
		var packet=stream.receive();
		if(packet)	processData(packet);
		if(game.timed && game.started && !game.finished) 
		{
			for(player in game.players)
			{
				if(game.turn==player && game.movelist.length)
				{
					var timer=game.players[player].timer;
					var current=time();
					if(timer.lastupdate===false) game.players[player].timer.lastupdate=current;
					if(timer.lastupdate!=current) 
					{
						var counted=timer.countdown(current);
						if(!counted && player==currentplayer) endGame("loss");
					}
				}
			}
			showTimers();
		}
	}
	function processData(packet)
	{
		debug(packet);
		if(packet.gameNumber != game.gameNumber) return false;
		switch(packet.func.toUpperCase())
		{
			case "MOVE":
				getMove(packet);
				game.movelist.push(packet);
				scanCheck();
				nextTurn();
				infoBar();
				if(game.turn==game.currentplayer)
				{
					notice("\1g\1hIt's your turn.");
				}
				else
				{
					notice("\1g\1hIt is " + getAlias(game.turn) + "'s turn.");
				}
				break;
			case "DRAW":
				log("received draw offer");
				if(draw()) {
					sendnotify("\1g\1hdraw offer accepted.");
				} else {
					sendnotify("\1g\1hdraw offer refused.");
					storeGame(game);
				}
				delete game.draw;
				clearAlert();
				break;
			case "CASTLE":
				getMove(packet);
				break;
			case "TILE":
				var newpiece=false;
				var update=game.board.grid[packet.x][packet.y];
				if(packet.contents) newpiece=new ChessPiece(packet.contents.name,packet.contents.color);
				update.contents=newpiece;
				update.draw(game.board.side,true);
				break;
			case "ALERT":
				log("received alert");
				for(m in packet.message) menuPrompt(packet.message[m]);
				menuPrompt("\1r\1h[Press any key]");
				while(console.inkey==="");
				chat.redraw();
				break;
			case "UPDATE":
				log("updating game data");
				game.loadGameTable();
				loadGameData();
				chessplayers.loadPlayers();
				infoBar();
				if(game.timed) showTimers();
				break;
			case "TIMER":
				var player=packet.player;
				var countdown=packet.countdown;
				game.players[player].timer.update(countdown);
				infoBar();
				break;
			default:
				log("Unknown chess data type received");
				log("packet: " + packet.toSource());
				break;
		}
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		game.board.drawBoard();
		infoBar();
		chat.redraw();
		drawLine(41,7,40,DARKGRAY);
		drawLine(41,23,40,DARKGRAY);
	}
	function packageData(data,func)
	{
		switch(func.toUpperCase())
		{
			case "ALERT":
				data.msg=data;
				break;
			case "MOVE":
			case "CASTLE":
			case "TILE":
			case "UPDATE":
				break;
			default:
				log("Unknown chess data not sent");
				break;
		}
		data.func=func;
		data.gameNumber=game.gameNumber;
		return data;
	}
	function send(data,type)
	{
		var d=packageData(data,type);
		stream.send(d);
	}
	function reverse()
	{
		game.board.reverse();
	}

	
/*	MOVEMENT FUNCTIONS	*/
	function tryMove()
	{
		game.board.clearLastMove();
		var moved=false;
		var from;
		var to;
		var placeholder;
		var reselect;
		while(1) {
			reselect=false;
			while(1) {
				from=selectTile(false,from);
				if(from===false) return false;
				else if(game.board.grid[from.x][from.y].contents && 
					game.board.grid[from.x][from.y].contents.color == game.turn) 
					break;
				else {
					notify("\1r\1hInvalid Selection");
				}
			}
			placeholder=from;
			while(1) {
				to=selectTile(from,placeholder);
				if(to===false) return false;
				else if(from.x==to.x && from.y==to.y) {
					reselect=true;
					break;
				} else if(!game.board.grid[to.x][to.y].contents || 
					game.board.grid[to.x][to.y].contents.color != game.turn) {
					if(checkMove(from,to)) {
						moved=true;
						break; 
					} else {
						placeholder=to;
						notify("\1r\1hIllegal Move");
					}
				} else {
					placeholder=to;
					notify("\1r\1hInvalid Selection");
				}
			}
			if(!reselect) break;
		}
		game.board.drawTile(from.x,from.y,moved);
		game.board.drawTile(to.x,to.y,moved);
		return true;
	}
	function checkMove(from,to)
	{ 
		var movetype=checkRules(from,to);
		if(!movetype) return false; //illegal move
		
		var from_tile=game.board.grid[from.x][from.y];
		var to_tile=game.board.grid[to.x][to.y];
		var move=new ChessMove(from,to,from_tile.contents.color);

		if(movetype=="en passant")
		{
			if(!enPassant(from_tile,to_tile,move)) return false;
		}
		else if(movetype=="pawn promotion")
		{
			if(!pawnPromotion(to_tile,move)) return false;
		}
		else if(movetype=="CASTLE")
		{
			if(!castle(from_tile,to_tile)) return false;
		}
		else
		{
			game.move(move);
			if(inCheck(game.board.findKing(currentplayer),currentplayer)) 
			{
				log("Illegal Move: King would be in check");
				game.unMove(move);
				return false;
			}
			sendMove(move);
		}
		infoBar();
		return true;
	}
	function sendMove(move)
	{
		nextTurn();
		notice("\1g\1hIt is " + getAlias(game.turn) + "'s turn.");
		notifyPlayer();
		game.board.lastmove=move;
		game.movelist.push(move);
		storeGame(game);
		send(move,"MOVE");
		if(game.timed) 
		{
			send(game.players[currentplayer].timer,"timer");
		}
	}
	function getMove(move)
	{
		game.move(move);
		game.board.clearLastMove();
		game.board.lastmove=move;
		game.board.drawTile(move.from.x,move.from.y,true,"\1r\1h");
		game.board.drawTile(move.to.x,move.to.y,true,"\1r\1h");
	}
	function scanCheck()
	{
		if(currentplayer)
		{
			var checkers=inCheck(game.board.findKing(currentplayer),currentplayer);
			if(checkers) 
			{
				if(findCheckMate(checkers,currentplayer)) 
				{
					endGame("loss");
					notice("\1r\1hCheckmate! You lose!");
				}
				else notice("\1r\1hYou are in check!");
			}
		}
	}
	function selectTile(start,placeholder)
	{
		if(start) {
			var sel=false;
			if(!placeholder) {
				selected={"x":start.x, "y":start.y};
				sel=true;
			}
			game.board.drawBlinking(start.x,start.y,sel,"\1n\1b");
		}
		if(placeholder) {
			selected={"x":placeholder.x, "y":placeholder.y};
			game.board.drawTile(selected.x,selected.y,true,"\1n\1b");
		} else {
			if(game.board.side=="white") selected={"x":0,"y":7};
			else selected={"x":7,"y":0};
			game.board.drawTile(selected.x,selected.y,true,"\1n\1b");
		}
		while(1) {
			cycle();
			var key=console.inkey(K_NOECHO|K_NOCRLF,50);
			if(key=="\r") {
				if(chat.input_line.buffer.length) Chat(key,chat);
				else return selected;
			}
			if(key)	{
				if(game.board.side=="white") {
					clearAlert();
					switch(key)
					{
						case KEY_DOWN:
						case "2":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case KEY_UP:
						case "8":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case KEY_LEFT:
						case "4":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							break;
						case KEY_RIGHT:
						case "6":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							break;
						case "7":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "9":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "1":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "3":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "\x1B":
							if(start) game.board.drawTile(start.x,start.y);
							game.board.drawTile(selected.x,selected.y);
							return false;
						default:
							if(!Chat(key,chat)) {
								if(start) game.board.drawTile(start.x,start.y);
								game.board.drawTile(selected.x,selected.y);
								return false;
							}
							continue;
					}
					if(start && start.x==selected.x && start.y==selected.y) 
						game.board.drawBlinking(start.x,start.y,true);
					else
						game.board.drawTile(selected.x,selected.y,true);
				} else {
					clearAlert();
					if(start) game.board.drawBlinking(start.x,start.y);
					switch(key)
					{
						case KEY_UP:
						case "2":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case KEY_DOWN:
						case "8":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case KEY_RIGHT:
						case "4":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							break;
						case KEY_LEFT:
						case "6":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							break;
						case "3":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "1":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "9":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "7":
							if(start && start.x==selected.x && start.y==selected.y) 
								game.board.drawBlinking(start.x,start.y);
							else
								game.board.drawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "\x1B":
							if(start) game.board.drawTile(start.x,start.y);
							game.board.drawTile(selected.x,selected.y);
							return false;
						default:
							if(!Chat(key,chat)) {
								if(start) game.board.drawTile(start.x,start.y);
								game.board.drawTile(selected.x,selected.y);
								return false;
							}
							continue;
					}
					if(start && start.x==selected.x && start.y==selected.y) 
						game.board.drawBlinking(start.x,start.y,true);
					else
						game.board.drawTile(selected.x,selected.y,true);
				}
			}
		}
	}
	function checkRules(from,to)
	{ 
		var from_tile=game.board.grid[from.x][from.y];
		var to_tile=game.board.grid[to.x][to.y];
		
		from.y=parseInt(from.y);
		from.x=parseInt(from.x);
		to.y=parseInt(to.y);
		to.x=parseInt(to.x);
		
		if(to_tile.contents && to_tile.contents.color==from_tile.contents.color) 
		{
			log("Invalid Move: Target Color same as Source Color");
			return false;
		}
		//KING RULESET
		if(from_tile.contents.name=="king") 
		{
			if(Math.abs(from.x-to.x)==2) 
			{
				if(inCheck(from,from_tile.contents.color))
				{
					log("Invalid Move: King is in check");
					return false;
				}
				if(from_tile.contents.has_moved) 
				{
					log("Invalid Move: King has already moved and cannot castle");
					return false;
				}
				if(from.x > to.x) 
				{
					var x=from.x-1;
					while(x>0) 
					{
						var spot_check={"x":x,"y":from.y};
						if(game.board.grid[x][from.y].contents) return false;
						if(inCheck(spot_check,from_tile.contents.color)) return false;
						x--;
					}
				}
				else 
				{
					var x=from.x+1;
					while(x<7) 
					{
						var spot_check={"x":x,"y":from.y};
						if(game.board.grid[x][from.y].contents) return false;
						if(inCheck(spot_check,from_tile.contents.color)) return false;
						x++;
					}
				}
				return "CASTLE";
			}
			else 
			{
				if(Math.abs(from.x-to.x)>1 || Math.abs(from.y-to.y)>1) 
				{
					log("Invalid Move: King can only move one space unless castling");
					return false;
				}
			}
		}
		//PAWN RULESET
		else if(from_tile.contents.name=="pawn") 
		{
			var xgap=Math.abs(from.x-to.x);
			var ygap=Math.abs(from.y-to.y);

			if(ygap>2 || ygap<1) return false; 
			if(xgap>1) return false; 
			if(from.x==to.x && to_tile.contents) return false; //CANNOT TAKE PIECES ON SAME VERTICAL LINE
			switch(from_tile.contents.color)
			{
				case "white":
					if(from.y<to.y) return false; //CANNOT MOVE BACKWARDS
					if(ygap==2)
					{
						if(from.y!=6) 
							return false;
						if(game.board.grid[from.x][from.y-1].contents) 
							return false;
					}
					if(to.y===0) return "pawn promotion";
					if(xgap==ygap && !to_tile.contents)
					{
						//EN PASSANT
						if(to.y==2) 
						{
							var lastmove=game.board.lastmove;
							if(lastmove.to.x!=to.x) 
							{ 
								return false; 
							}
							var lasttile=game.board.grid[lastmove.to.x][lastmove.to.y];
							if(Math.abs(lastmove.from.y-lastmove.to.y)!=2)
							{ 
								return false; 
							}
							if(lasttile.contents.name!="pawn")
							{ 
								return false; 
							}
							return "en passant";
						}
						else return false;
					}
					break;
				case "black":
					if(from.y>to.y) return false; //CANNOT MOVE BACKWARDS
					if(ygap==2)
					{
						if(from.y!=1) return false;
						if(game.board.grid[from.x][from.y+1].contents) return false;
					}
					if(to.y==7) return "pawn promotion";
					if(xgap==ygap && !to_tile.contents)
					{
						//EN PASSANT
						if(to.y==5)
						{
							var lastmove=game.board.lastmove;
							if(lastmove.to.x!=to.x) 
							{ 
								return false; 
							}
							var lasttile=game.board.grid[lastmove.to.x][lastmove.to.y];
							if(Math.abs(lastmove.from.y-lastmove.to.y)!=2)
							{ 
								return false; 
							}
							if(lasttile.contents.name!="pawn")
							{ 
								return false; 
							}
							return "en passant";
						}
						else return false;
					}
					break;
			}
		}
		//KNIGHT RULESET
		else if(from_tile.contents.name=="knight") 
		{
			if(Math.abs(from.x-to.x)==2 && Math.abs(from.y-to.y)==1);
			else if(Math.abs(from.y-to.y)==2 && Math.abs(from.x-to.x)==1);
			else return false;
		}
		//ROOK RULESET
		else if(from_tile.contents.name=="rook") 
		{
			if(from.x==to.x) 
			{
				if(from.y<to.y) 
				{
					var distance=to.y-from.y;
					for(check = 1;check<distance;check++) 
					{
						if(game.board.grid[from.x][from.y+check].contents) return false;
					}
				}
				else 
				{
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) 
					{
						if(game.board.grid[from.x][from.y-check].contents) return false;
					}
				}
			}
			else if(from.y==to.y) 
			{
				if(from.x<to.x) 
				{
					var distance=to.x-from.x;
					for(check = 1;check<distance;check++) 
					{
						if(game.board.grid[from.x+check][from.y].contents) return false;
					}
				}
				else 
				{
					var distance=from.x-to.x;
					for(check = 1;check<distance;check++)
					{
						if(game.board.grid[from.x-check][from.y].contents) return false;
					}
				}
			}
			else return false;
		}
		//BISHOP RULESET
		else if(from_tile.contents.name=="bishop") 
		{
			diffx=from.x-to.x;
			diffy=from.y-to.y;
			if(Math.abs(diffx)==Math.abs(diffy)){
				var distance=Math.abs(diffx);
				var checkx=from.x;
				var checky=from.y;
				for(check=1;check<distance;check++) {
					if(diffx<0) checkx++;
					else checkx--;
					if(diffy<0) checky++;
					else checky--;
					log("checking space: " + checkx + "," + checky);
					if(game.board.grid[checkx][checky].contents) return false;
				}
			}
			else return false;
		}
		//QUEEN RULESET
		else if(from_tile.contents.name=="queen") 
		{
			diffx=from.x-to.x;
			diffy=from.y-to.y;
			if(Math.abs(diffx)==Math.abs(diffy))
			{
				var distance=Math.abs(diffx);
				var checkx=from.x;
				var checky=from.y;
				for(check=1;check<distance;check++) 
				{
					if(diffx<0) checkx++;
					else checkx--;
					if(diffy<0) checky++;
					else checky--;
					if(game.board.grid[checkx][checky].contents) return false;
				}
			}
			else if(from.x==to.x) 
			{
				if(from.y<to.y) 
				{
					var distance=to.y-from.y;
					for(check = 1;check<distance;check++) 
					{
						if(game.board.grid[from.x][parseInt(from.y,10)+check].contents) return false;
					}
				}
				else 
				{
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) 
					{
						if(game.board.grid[from.x][from.y-check].contents) return false;
					}
				}
			}
			else if(from.y==to.y) 
			{
				if(from.x<to.x) 
				{
					var distance=to.x-from.x;
					for(check = 1;check<distance;check++) 
					{
						if(game.board.grid[parseInt(from.x,10)+check][from.y].contents) return false;
					}
				}
				else 
				{
					var distance=from.x-to.x;
					for(check = 1;check<distance;check++) 
					{
						if(game.board.grid[parseInt(from.x)-check][from.y].contents) return false;
					}
				}
			}
			else return false;
		}

		return true;
	}
	function enPassant(from,to,move)
	{
		log("trying en passant");
		var row=(to.y<from.y?to.y+1:to.y-1);
		var cleartile=game.board.grid[to.x][row];
		var temp=cleartile.contents;
		delete cleartile.contents;
		game.move(move);
		if(inCheck(game.board.findKing(currentplayer),currentplayer)) 
		{
			log("restoring original piece");
			game.unMove(move);
			cleartile.contents=temp;
			return false;
		}
		game.board.drawTile(cleartile.x,cleartile.y);
		send(cleartile,"UPDATETILE");
		sendMove(move);
		return true;
	}
	function castle(from,to)
	{
		log("trying castle");
		var rfrom;
		var rto;
		if(from.x-to.x==2) 
		{	//QUEENSIDE
			rfrom={'x':to.x-2,'y':to.y};
			rto={'x':to.x+1,'y':to.y};	
		}
		else 
		{	//KINGSIDE
			rfrom={'x':to.x+1,'y':to.y};
			rto={'x':to.x-1,'y':to.y};
		}
		var castle=new ChessMove(rfrom,rto);
		var move=new ChessMove(from,to);
		game.move(castle);
		game.move(move);
		send(castle,"CASTLE");
		sendMove(move);
		game.board.drawTile(rfrom.x,rfrom.y,false);
		game.board.drawTile(rto.x,rto.y,false);
		return true;
	}
	function pawnPromotion(to_tile,move)
	{
		game.move(move);
		if(inCheck(game.board.findKing(currentplayer),currentplayer)) 
		{
			game.unMove(move);
			return false;
		}
		sendMove(move);
		to_tile.contents=new ChessPiece("queen",to_tile.contents.color);
		send(to_tile,"UPDATETILE");
	}
	function findCheckMate(checkers,player)
	{
		var position=game.board.findKing(player);
		if(kingCanMove(position,player)) return false;
		log("king cannot move");
		for(checker in checkers) 
		{
			var spot=checkers[checker];
			log("checking if " + player + " can take or block " +  spot.contents.color + " " + spot.contents.name);
			if(spot.contents.name=="knight" || spot.contents.name=="pawn") 
			{
				var canmove=canMoveTo(spot,player);
				//if king cannot move and no pieces can take either the attacking knight or pawn - checkmate!
				if(!canmove) return true;
				//otherwise attempt each move, then scan for check - if check still exists after all moves are attempted - checkmate!
				for(piece in canmove)
				{
					var move=new ChessMove(canmove[piece],spot);
					game.move(move);
					if(inCheck(position,player)) 
					{
						game.unMove(move);
					}
					//if player is no longer in check after move is attempted - game continues
					else 
					{
						game.unMove(move);
						return false;
					}						
				}
			}
			else 
			{
				path=findPath(spot,position);
				for(p in path)
				{
					var pos=path[p];
					var canmove=canMoveTo(pos,player);
					if(canmove)
					{
						for(piece in canmove)
						{
							log("attempting block move: " + canmove[piece].contents.name);
							var move=new ChessMove(canmove[piece],pos);
							game.move(move);
							if(inCheck(position,player)) 
							{
								game.unMove(move);
							}
							else 
							{
								game.unMove(move);
								return false;
							}						
						}
					}
				}
			}
		}
		return true;
	}
	function findPath(from,to)
	{
		var path=[];
		var incrementx=0;
		var incrementy=0;
		if(from.x!=to.x)
		{
			incrementx=(from.x>to.x?-1:1);
		}
		if(from.y!=to.y)
		{
			incrementy=(from.y>to.y?-1:1);
		}
		var x=from.x;
		var y=from.y;
		while(x!=to.x || y!=to.y)
		{
			log("adding target x" + x + "y" + y);
			path.push({"x":x,"y":y});
			x+=incrementx;
			y+=incrementy;
		}
		return path;
	}
	function canMoveTo(position,player)
	{
		var pieces=game.board.findAllPieces(player);
		var canmove=[];
		for(p in pieces)
		{
			var piece=pieces[p];
			var gridposition=game.board.grid[piece.x][piece.y];
			if(gridposition.contents.name=="king")
			{
				log("skipping king, moves already checked");
			}
			else if(checkRules(piece,position))
			{
				log("piece can cancel check: " + gridposition.contents.color + " " + gridposition.contents.name);
				canmove.push(piece);
			}
		}
		if(canmove.length) return canmove;
		return false;
	}
	function kingCanMove(king,player)
	{
		log("checking if king can move from x: " + king.x + " y: " + king.y);
		var north=		(king.y>0?					{"x":king.x,	"y":king.y-1}:false);
		var northeast=	(king.y>0 && king.x<7?		{"x":king.x+1,	"y":king.y-1}:false);
		var northwest=	(king.y>0 && king.x>0?		{"x":king.x-1,	"y":king.y-1}:false);
		var south=		(king.y<7?					{"x":king.x,	"y":king.y+1}:false);
		var southeast=	(king.y<7 && king.x<7?		{"x":king.x+1,	"y":king.y+1}:false);
		var southwest=	(king.y<7 && king.x>0?		{"x":king.x-1,	"y":king.y+1}:false);
		var east=		(king.x<7?					{"x":king.x+1,	"y":king.y}:false);
		var west=		(king.x>0?					{"x":king.x-1,	"y":king.y}:false);
		
		var directions=[north,northeast,northwest,south,southeast,southwest,east,west];
		for(dir in directions) 
		{
			var direction=directions[dir];
			if(direction && checkRules(king,directions[dir])) 
			{
				var move=new ChessMove(king,direction,player);
				game.move(move);
				if(inCheck(direction,player)) 
				{
					log("taking piece would cause check");
					game.unMove(move);
				}
				else
				{
					log("king can move to x: " + direction.x + " y: " + direction.y);
					game.unMove(move);
					return true;
				}
			}
		}
		return false;
	}
	function inCheck(position,player)
	{
		var check_pieces=[];
		for(column in game.board.grid) 
		{ 
			for(var row in game.board.grid[column]) 
			{
				if(game.board.grid[column][row].contents)
				{
					if(game.board.grid[column][row].contents.name=="king");
					else if(game.board.grid[column][row].contents.color!=player) 
					{
						var from={"x":column, "y":row};
						if(checkRules(from,position)) 
						{
							log(game.board.grid[column][row].color + " " + game.board.grid[column][row].contents.name + " at " + column + "," + row + " can take king");
							check_pieces.push(game.board.grid[column][row]);
						}
					}
				}
			}
		}
		if(check_pieces.length) return check_pieces;
		else return false;
	}
	function draw()
	{
		notify("\1c\1hOpponent offered a draw. Accept? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
		endGame("DRAW");
		return true;
	}

	/*	GAMEPLAY FUNCTIONS	*/
	function playerInGame()
	{
		return game.playerInGame();
	}
	function offerDraw()
	{
		notify("\1c\1hOffer your opponent a draw? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
		game.draw=currentplayer;
		storeGame(game);
		send(new Packet("DRAW"),"DRAW");
	}
	function newGame()
	{
		//TODO: Add the ability to change game settings (timed? rated?) when restarting/rematching
		game.newGame();
		send(new Packet("UPDATE"),"UPDATE");
	}
	function resign()
	{
		notify("\1c\1hAre you sure? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return;
		endGame("loss");
	}
	function endGame(outcome)
	{
		log("Ending game - " + currentplayer + ": " + outcome);
		if(game.movelist.length) 
		{
			var cp=currentplayer;
			
			var side1=(cp=="white"?"white":"black");
			var side2=(cp=="white"?"black":"white");
			
			var id1=game.players[side1].id;
			var id2=game.players[side2].id;
			
			var p1=chessplayers.players[id1];
			var p2=chessplayers.players[id2];
			
			var r1=p1.rating;
			var r2=p2.rating;
			
			switch(outcome)
			{
				case "win":
					p1.wins+=1;
					p2.losses+=1;
					break;
				case "loss":
					p1.losses+=1;
					p2.wins+=1;
					break;
				case "DRAW":
					p1.draws+=1;
					p2.draws+=1;
					break;
			}
			if(game.rated)
				switch(outcome)
				{
					case "win":
						p1.rating=getRating(p1.rating,p2.rating,"win");
						p2.rating=getRating(p2.rating,p1.rating,"loss");
						break;
					case "loss":
						p1.rating=getRating(p1.rating,p2.rating,"loss");
						p2.rating=getRating(p2.rating,p1.rating,"win");
						break;
					case "DRAW":
						p1.rating=getRating(p1.rating,p2.rating,"DRAW");
						p2.rating=getRating(p2.rating,p1.rating,"DRAW");
						break;
				}
			
			chessplayers.storePlayer(id1);
			chessplayers.storePlayer(id2);
		}
		delete game.turn;
		game.started=false;
		game.finished=true;
		storeGame(game);
		infoBar();
		send(new Packet("UPDATE"),"UPDATE");
	}
	function showNewRatings(p1,p2)
	{
		notice("\1c\1hGame Completed");
		notice("\1c\1h" + p1.name + "'s new rating: " + p1.rating);
		notice("\1c\1h" + p2.name + "'s new rating: " + p2.rating);
		notify("\1r\1h[Press any key]");
		console.getkey();
	}
	function joinGame()
	{
		if(game.players.white.id) 
		{
			addPlayer("black",user.alias);
			startGame("black");
		}
		else if(game.players.black.id) 
		{
			addPlayer("white",user.alias);
			startGame("white");
		}
		else 
		{
			chat.chatroom.alert("\1nChoose a side [\1hW\1n,\1hB\1n]: ");
			var color=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
			switch(color)
			{
				case "W":
					addPlayer("white",user.alias);
					break;
				case "B":
					addPlayer("black",user.alias);
					break;
				default:
					return;
			}
			storeGame(game);
		}
		currentplayer=game.currentplayer;
		send(new Packet("UPDATE"),"UPDATE");
		redraw();
	}
	function listMoves()
	{
		notice("\1c\1hMOVE HISTORY");
		for(m=0;m<game.movelist.length;m++)
		{	
			var move=game.movelist[m];
			var text=game.movelist[m].format();
			m++;
			if(game.movelist[m])
			{
				text+=("  " + game.movelist[m].format());
			}
			notice(text);
		}
		notify("\1r\1hPress a key to continue");
		console.getkey();
	}
	function getRating(r1,r2,outcome)
	{
		//OUTCOME SHOULD BE RELATIVE TO PLAYER 1 (R1 = PLAYER ONE RATING)
		//WHERE PLAYER 1 IS THE USER RUNNING THE CURRENT INSTANCE OF THE SCRIPT
		//BASED ON "ELO" RATING SYSTEM: http://en.wikipedia.org/wiki/ELO_rating_system#Mathematical_details
		
		
		var K=(r1>=2400?16:32); //USE K-FACTOR OF 16 FOR PLAYERS RATED 2400 OR GREATER
		var points;
		switch(outcome)
		{
			case "win":
				points=1;
				break;
			case "loss":
				points=0;
				break;
			case "DRAW":
				points=0.5;
				break;
		}
		var expected=1/(1+Math.pow(10,(r2-r1)/400));
		var newrating=Math.round(r1+K*(points-expected));
		return newrating;
	}

/*	GAME DATA FUNCTIONS	*/
	function getAlias(color)
	{
		var playerfullname=game.players[color].id;
		if(playerfullname)
		{
			return(chessplayers.players[playerfullname].name);
		}
		return "[Empty Seat]";
	}
	function startGame(currentplayer)
	{
		game.currentplayer=currentplayer;
		game.started=true;
		game.turn="white";
		game.board.side=currentplayer;
		storeGame(game);
	}
	function addPlayer(color,player)
	{
		var fullname=chessplayers.getFullName(player);
		game.players[color].id=fullname;
		game.currentplayer=color;
		game.board.side=color;
	}
	function loadGameData()
	{
		game.loadGameTable();
		var gFile=new File(game.gamefile);
		if(!file_exists(gFile.name)) return false;
		gFile.open("r");
		
		game.movelist=[];
		var lastmove=gFile.iniGetObject("lastmove");
		if(lastmove.from)
		{
			game.board.lastmove=new ChessMove(getChessPosition(lastmove.from),getChessPosition(lastmove.to));
		}
		var draw=gFile.iniGetValue(null,"DRAW");
		if(draw)
		{
			game.draw=draw;
		}
		
		//LOAD PIECES
		var pieces=gFile.iniGetAllObjects("position","board.");
		for(p in pieces)
		{
			var pos=getChessPosition(pieces[p].position);
			var name=pieces[p].piece;
			var color=pieces[p].color;
			game.board.grid[pos.x][pos.y].addPiece(new ChessPiece(name,color));
		}
		//LOAD MOVES
		var moves=gFile.iniGetAllObjects("number","move.");
		for(move in moves)
		{
			var from=getChessPosition(moves[move].from);
			var to=getChessPosition(moves[move].to);
			var color=moves[move].color;
			var check=moves[move].check;
			game.movelist.push(new ChessMove(from,to,color,check));
		}
		gFile.close();
	}
	function nextTurn()
	{
		game.turn=(game.turn=="white"?"black":"white");
	}
	function notifyPlayer()
	{
		var nextturn=game.players[game.turn];
		if(!chat.findUser(nextturn.id))
		{
			var uname=chessplayers.players[nextturn.id].name;
			var unum=system.matchuser(uname);
			var message="\1n\1yIt is your turn in \1hChess\1n\1y game #\1h" + game.gameNumber + "\r\n\r\n";
			system.put_telegram(unum, message);
			//TODO: make this handle interbbs games if possible
		}
	}
	
	initGame();
	initChat();
	initMenu();
	main();
}

function notify(message)
{
	chat.chatroom.alert(message);
}
function notice(message)
{
	chat.chatroom.notice(message);
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
	gFile.iniSetValue(null,"gameNumber",game.gameNumber);
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
		gFile.iniSetValue(null,"DRAW",game.draw);
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
function getChessPosition(position)
{
	var letters="abcdefgh";
	if(typeof position=="string")
	{
		//CONVERT BOARD POSITION IDENTIFIER TO XY COORDINATES
		var pos={x:letters.indexOf(position.charAt(0)),y:8-(parseInt(position.charAt(1),10))};
		return(pos);
	}
	else if(typeof position=="object")
	{
		//CONVERT XY COORDINATES TO STANDARD CHESS BOARD FORMAT
		return(letters.charAt(position.x)+(8-position.y));
	}
}

lobby();
