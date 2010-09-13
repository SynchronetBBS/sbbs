/*
	JAVASCRIPT TETRIS INTERBBS
	For Synchronet v3.15+
	Matt Johnson(2009)
*/

load("chateng.js");
load("graphic.js");
load("sbbsdefs.js");
load("funclib.js");

//load("helpfile.js"); //TODO: write instructions
var root=js.exec_dir;
load(root + "tetrisobj.js");
load(root + "menu.js");
load(root + "timer.js");

var stream=new ServiceConnection("tetris");
var chat=new ChatEngine(root);
var players=new PlayerList();
var oldpass=console.ctrl_key_passthru;

//BUG REPORT: continuous dropping pieces when you should be dead keeps you alive

function splashStart()
{
	console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
	bbs.sys_status|=SS_MOFF;
	bbs.sys_status |= SS_PAUSEOFF;	
	console.clear();
	//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
	getFiles();
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

/*	main */
function lobby()
{
	var table_graphic=new Graphic(10,5);
	var lobby_graphic=new Graphic(80,24);
	var scrollBar=new Scrollbar(3,24,35,"horizontal","\1y");

	var refresh=true;
	var scroll_index=0;
	var last_table_number=0;
	var table_markers=[];
	var tables=[];
	var menu;

	function init()
	{
		table_graphic.load(root+"icon.bin");
		lobby_graphic.load(root+"lobby.bin");
		processGraphic();
		loadPlayers();
		updateTables();
		initChat();
		initMenu();
	}
	function initChat()
	{
		chat.init(38,19,42,3);
		chat.input_line.init(42,23,38,"","\1n");
		chat.joinChan("tetris lobby",user.alias,user.name);
	}
	function initMenu()
	{
		menu=new Menu("\1n","\1c\1h");
		var menu_items=[		"~New Game"		,
								"~Join Game"	,
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
		if(!tables.length) menu.disable(["J"]);
		else menu.enable(["J"]);
	}
	function getTablePointers()
	{
		var pointers=[];
		for(t in tables) {
			pointers.push(t);
		}
		return pointers;
	}
	function loadPlayers()
	{
		players.loadPlayers();
	}
	function updateTables()
	{
		var game_files=directory(root + "tetris*.ini");
		for(var i=0;i<game_files.length;i++) {
			var filename=file_getname(game_files[i]);
			var gameNumber=Number(filename.substring(6,filename.indexOf(".")));
			if(tables[gameNumber]) {
				var lastupdate=file_date(game_files[i]);
				var lastloaded=tables[gameNumber].lastupdate;
				if(lastupdate>lastloaded) {
					log("updating game: " +  gameNumber);
					tables[gameNumber].loadGame();
				}
			} else {
				log("loading game: " + game_files[i]);
				loadTable(game_files[i]);
			}
		}
		for(t in tables) {
			var table=tables[t];
			if(table && !file_exists(table.dataFile)) {
				log("removing deleted game: " + table.dataFile);
				delete tables[t];
				refresh=true;
			}
		}
	}
	function loadTable(dataFile,index)
	{
		var table=new Tetris(dataFile);
		if(table.gameNumber>last_table_number) last_table_number=table.gameNumber;
		tables[table.gameNumber]=table;
	}
	function listTables(full)
	{
		var pointers=getTablePointers();
		var range=(pointers.length%2==1?pointers.length+1:pointers.length);
		if(tables.length>4) scrollBar.draw(scroll_index,range);

		var index=scroll_index;
		for(i in table_markers) {
			var x=table_markers[i].x;
			var y=table_markers[i].y;
			if(full) {
				clearBlock(x,y,19,10);
			}
			var pointer=pointers[index];
			if(tables[pointer])	{
				if(full || tables[pointer].update) {
					var game=tables[pointer];
					table_graphic.draw(x,y);
					listGameInfo(x,y,game);
				}
				index++;
			}
		}
		console.attributes=ANSI_NORMAL;
	}
	function listGameInfo(x,y,game)
	{
		console.gotoxy(x+10,y);
		console.putmsg("\1n\1yGAME\1h: " + game.gameNumber);
		
		console.gotoxy(x+11,y+2);
		console.pushxy();
		if(game.status == 1) {
			menuPrompt(printPadded("\1n\1gGAME",8));
			menuPrompt(printPadded("\1n\1gIN",8));
			menuPrompt(printPadded("\1n\1gPROGRESS",8));
		} else	if(game.status == 0) {
			menuPrompt(printPadded("\1n\1gSTARTING",8));
			menuPrompt(printPadded("\1n\1gIN \1h" + parseInt(game.timer.countdown,10),8));
			menuPrompt(printPadded("\1n\1gSECONDS",8));
		} else	if(game.status == 2) {
			menuPrompt(printPadded("\1n\1gCOMPLETE",8));
		} else {
			menuPrompt(printPadded("\1n\1gWAITING",8));
			menuPrompt(printPadded("\1n\1gFOR",8));
			menuPrompt(printPadded("\1n\1gPLAYERS",8));
		}
		
		console.gotoxy(x,y+6);
		console.pushxy();
		console.putmsg("\1n\1yPLAYERS\1h:");
		for(var p in game.players)	{
			console.popxy();
			console.down();
			console.pushxy();
			console.putmsg("\1y\1h " + game.players[p].name);
		}
		game.update=false;
	}
	function drawLobby()
	{
		lobby_graphic.draw();
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		drawLobby();
		listTables(true);
		chat.redraw();
	}

	function main()
	{
		redraw();
		while(1) {
			cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,25);
			if(k) {
				var range=(tables.length%2==1?tables.length+1:tables.length);
				switch(k.toUpperCase())
				{
				case KEY_LEFT:
					if(scroll_index>0 && tables.length>4) {
						scroll_index-=2;
						listTables();
					}
					break;
				case KEY_RIGHT:
					if((scroll_index+1)<=range && tables.length>4) {
						scroll_index+=2;
						listTables();
					}
					break;
				case "/":
					if(!chat.input_line.buffer.length) {
						lobbyMenu();
						chat.redraw();
					}
					else if(!Chat(k,chat)) return;
					break;
				case "\x1b":	
					chat.partChan("tetris lobby",user.alias);
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
		chat.cycle();
		updateTables();
		var update=false;

		for each(var t in tables) {
			switch(Number(t.status)) {
			case -1:
				if(countMembers(t.players)>0) {
					initTimer(t);
					t.update=true;
					update=true;
				}
				break;
			case 0:
				if(!t.timer.countdown) t.loadGame();
				var current=time();
				var difference=current-t.timer.lastupdate;
				if(!difference>=1) break;
				if(!t.timer.countDown(difference)) {
					startGame(t);
				}
				t.update=true;
				update=true;
				break;
			case 1:
				var id=players.getPlayerID(user.alias);
				if(t.players[id] && t.players[id].active == true) {
					playGame(t);
					redraw();
				}
				break;
			case 2:
				break;
			default:
				log("unknown game status: " + t.status);
				mswait(500);
				break;
			}
		}
		if(refresh) {
			listTables(true);
			refresh=false;
		} else if(update) {
			listTables(false);
			update=false;
		}
		mswait(5);
	}
	function startGame(game)
	{
		log("starting game: " + game.gameNumber);
		game.status=1;
		storeGame(game);
	}
	function initTimer(game)
	{
		log("starting time for game: " + game.gameNumber);
		game.status=0;
		var file=new File(game.dataFile);
		file.open('r+',true);
		file.iniSetValue(null,"created",system.timer);
		file.iniSetValue(null,"status",0);
		file.close();
	}
	function lobbyMenu()
	{
		refreshCommands();
		listCommands();
		menuPrompt("\1nMake a selection: ");
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_UPPER);
		if(k) {
			if(menu.items[k] && menu.items[k].enabled) {
				switch(k.toUpperCase())
				{
				case "N":
					createNewGame();
					break;
				case "R":
					showRankings();
					redraw();
					break;
				case "H":
					help();
					break;
				case "D":
					redraw();
					break;
				case "J":
					var g=selectGame();
					if(g) joinGame(tables[g],user.alias);
					break;
				default:
					break;
				}
			}
		}
	}
	function showRankings()
	{
	}
	function joinGame(game,name)
	{
		var id=players.getPlayerID(name);
		if(game.players[id]) {
			notify("You are already in that game!");
			return false;
		}
		if(game.status>0) {
			notify("It has already started!");
			return false;
		}
		if(countMembers(game.players) == 3) {
			notify("That game is full!");
			return false;
		}
		game.players[id]=new Player(name,true);
		game.update=true;
		storePlayerData(game,id);
	}	
	function selectGame()
	{
		menuPrompt("\1nEnter Table \1h#: ");
		var num=console.getkeys("\x1bQ\r",last_table_number);
		if(tables[num]) {
			return num;
		}
		else return false;
	}
	function createNewGame()
	{
		var newgame=new Tetris();
		notice("\1g\1hGame #" + parseInt(newgame.gameNumber,10) + " created");
		var id=players.getPlayerID(user.alias);
		newgame.players[id]=new Player(user.alias,true);
		refresh=true;
		storeGame(newgame);
	}
 
	init();
	main();
}
function playGame(game) 
{
	var menu;
	var lastupdate=system.timer;
	
	/*	lines per level */
	var lines=10;
	/*  wait between gravitational movement */
	var pause=1; 
	/* 	pause multiplier for level increase */
	var pause_reduction=0.9
	/*	variable for holding outbound garbage quantity */
	var garbage=0;
	/* base point multiplier */
	var base_points=5;
	
	var currentPlayerID=players.getPlayerID(user.alias);
	var currentPlayer=false;

	function initGame()
	{
		var x=1;
		var y=3;
		var index=0;
		var width=27;
		if(game.players[currentPlayerID]) {
			game.players[currentPlayerID]=new GameBoard(user.alias,x,y);
			index++;
		}
		for(var p in game.players) {
			if(p == currentPlayerID) continue;
			game.players[p]=new GameBoard(game.players[p].name,x + (index*width),y);
			index++;
		}
		currentPlayer=game.players[currentPlayerID];
	}
	function initChat()
	{
		chat.init(38,15,42,8);
		chat.input_line.init(42,24,38,"","\1n");
		chat.partChan("tetris lobby",user.alias);
		chat.joinChan("tetris game " + game.gameNumber,user.alias,user.name);
	}
	function main()
	{
		redraw();
		while(game.status == 1) {
			cycle();
			mswait(5);
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
			switch(k)
			{
			case "\x1b":	
			case "Q":
				if(handleExit()) {
					return true;
				}
				break;
			}
			if(!currentPlayer.active || !currentPlayer.currentPiece) {
				continue;
			}
			if(checkInterference()) {
				killPlayer(currentPlayerID);
				send("DEAD");
				continue;
			}
			switch(k)
			{
			case " ":
			case "0":
				if(!currentPlayer.swapped) {
					currentPlayer.swapped=true;
					swapPiece();
				}
				break;
			case "8":
			case KEY_UP:
				fullDrop();
				send("MOVE");
				setPiece();
				break;
			case "2":
			case KEY_DOWN:
				if(move("DOWN")) {
					send("MOVE");
				} else {
					setPiece();
				}
				break;
			case "4":
			case KEY_LEFT:
				if(move("LEFT")) send("MOVE");
				break;
			case "6":
			case KEY_RIGHT:
				if(move("RIGHT")) send("MOVE");
				break;
			case "5":
				if(rotate("RIGHT")) send("MOVE");
				break;
			case "7":
			case "A":
				if(rotate("LEFT")) send("MOVE");
				break;
			case "9":
			case "D":
				if(rotate("RIGHT")) send("MOVE");
				break;
			default:
				break;
			}
		}
		console.gotoxy(currentPlayer.x+1,currentPlayer.y+11);
		console.pushxy();
		if(game.winner == currentPlayerID) {
			message("\1g\1hYOU WIN!");
		} else {
			message("\1r\1hYOU LOSE!");
		}
		message("\1n\1rpress \1hQ \1n\1rto quit");
		while(1) {
			if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)=="Q") {
				return;
			}
		}
	}
	function handleExit()
	{
		if(game.status == 1) {
			display(currentPlayer,"\1c\1hQuit game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
			if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") {
				redraw();
				return false;
			}
		}
		killPlayer(currentPlayerID);
		send("DEAD");
		if(game.status == 2 && game.winner == currentPlayerID) {
			if(file_exists(game.dataFile.name))
				file_remove(game.dataFile.name);
		}
		return true;
	}
	function cycle()
	{
		//chat.cycle();
		var packet=stream.receive();
		if(packet)	processData(packet);
		if(!currentPlayer || !currentPlayer.active) return;
		
		var update=false;
		var difference=system.timer-lastupdate;

		if(difference<pause) return;
		lastupdate=system.timer;

		getLines();

		if(!currentPlayer.currentPiece) {
			getNewPiece();
			return;
		}
		
		if(checkInterference()) {
			killPlayer(currentPlayerID);
			send("DEAD");
			return;
		} 
		
		if(move("DOWN")) {
			send("MOVE");
		} else {
			setPiece();
		}
	}
	function processData(packet)
	{
		if(packet.gameNumber != game.gameNumber) return false;
		switch(packet.func.toUpperCase())
		{
			case "MOVE":
				if(!game.players[packet.player].currentPiece) {
					log("received invalid move data");
					break;
				}
				game.players[packet.player].unDrawPiece();
				game.players[packet.player].currentPiece.x=packet.x;
				game.players[packet.player].currentPiece.y=packet.y;
				game.players[packet.player].currentPiece.o=packet.o;
				game.players[packet.player].drawPiece();
				break;
			case "PIECE":
				game.players[packet.player].currentPiece=loadPiece(packet.piece);
				game.players[packet.player].drawPiece();
				break;
			case "GARBAGE":
				currentPlayer.clearBoard();
				loadGarbage(packet.lines);
				currentPlayer.drawBoard();
				currentPlayer.drawPiece();
				break;
			case "DEAD":
				killPlayer(packet.player);
				break;
			case "NEXT":
				game.players[packet.player].nextPiece=packet.piece;
				game.players[packet.player].drawNext();
				break;
			case "HOLD":
				game.players[packet.player].holdPiece=packet.piece;
				game.players[packet.player].drawHold();
				break;
			case "SCORE":
				game.players[packet.player].score=packet.score;
				game.players[packet.player].lines=packet.lines;
				game.players[packet.player].level=packet.level;
				game.players[packet.player].drawScore();
				break;
			case "GRID":
				game.players[packet.player].grid=packet.grid;
				game.players[packet.player].clearBoard();
				game.players[packet.player].drawBoard();
				break;
			case "UPDATE":
				log("updating game data");
				game.loadGame();
				break;
			default:
				log("Unknown tetris data type received");
				log("packet: " + packet.toSource());
				break;
		}
	}
	function packageData(func)
	{
		var data=new Packet(func);
		switch(func.toUpperCase())
		{
			case "MOVE":
				data.x=currentPlayer.currentPiece.x;
				data.y=currentPlayer.currentPiece.y;
				data.o=currentPlayer.currentPiece.o;
				break;
			case "PIECE":
				data.piece=currentPlayer.currentPiece.id;
				break;
			case "NEXT":
				data.piece=currentPlayer.nextPiece;
				break;
			case "HOLD":
				data.piece=currentPlayer.holdPiece;
				break;
			case "DEAD":
				break;
			case "GARBAGE":
				data.lines=garbage;
				break;
			case "SCORE":
				data.score=currentPlayer.score;
				data.lines=currentPlayer.lines;
				data.level=currentPlayer.level;
				break;
			case "UPDATE":
				break;
			case "GRID":
				data.grid=currentPlayer.grid;
				break;
			default:
				log("Unknown tetris data not sent");
				break;
		}
		data.gameNumber=game.gameNumber;
		data.player=currentPlayerID;
		return data;
	}
	function send(func)
	{
		var d=packageData(func);
		if(d) stream.send(d);
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		for each(var p in game.players) {
			p.drawBackground();
			p.drawScore();
			p.drawBoard();
		}
		//chat.redraw();
	}

	/*	gameplay functions */
	function loadGarbage(lines)
	{
		var space=random(10);
		while(lines>0) {
			if(currentPlayer.currentPiece) {
				currentPlayer.currentPiece.y++;
				if(checkInterference()) {
					currentPlayer.currentPiece.y--;
					setPiece();
				} else {
					currentPlayer.currentPiece.y--;
				}
			}
			var topLine=currentPlayer.grid.shift();
			for(var x=0;x<topLine.length;x++) {
				if(topLine[x] > 0) {
					log("garbage overflow");
					killPlayer(currentPlayerID);
					return;
				}
			}
			currentPlayer.grid.push(getGarbageLine(space));
			lines--;
		}
		send("GRID");
	}
	function fullDrop()
	{	
		while(1) {
			if(!move("DOWN")) break;
		}
	}
	function getLines()
	{
		var lines=[];
		for(var y=0;y<currentPlayer.grid.length;y++) {
			var line=true;
			for(var x=0;x<currentPlayer.grid[y].length;x++) {
				if(currentPlayer.grid[y][x] == 0) {
					line=false;
					break;
				}
			}
			if(line) {
				currentPlayer.grid.splice(y,1);
				currentPlayer.grid.unshift(getNewLine(currentPlayer.grid[0].length));
				lines.push(y);
			}
		}
		if(lines.length>0) {
			currentPlayer.clearBoard();
			currentPlayer.drawBoard();
			send("GRID");
			
			scoreLines(lines.length);

			if(game.garbage) {
				if(lines.length==4) {
					garbage=4;
				} else if(lines.length==3) {
					garbage=2;
				} else if(lines.length==2) {
					garbage=1;
				} else {
					garbage=0;
				}
			} 
			if(garbage>0) send("GARBAGE");
		}
	}
	function scoreLines(qty)
	{
		/*	a "TETRIS" */
		if(qty==4) qty=10;

		currentPlayer.lines+=qty;
		currentPlayer.score+=(currentPlayer.level * qty * base_points);

		if(currentPlayer.lines/10 >= currentPlayer.level) {
			currentPlayer.level++;
			pause=pause*pause_reduction;
		}
		
		currentPlayer.drawScore();
		send("SCORE");
	}
	function setPiece()
	{
		var piece=currentPlayer.currentPiece;
		var piece_matrix=piece.orientation[currentPlayer.currentPiece.o];
		
		var yoffset=piece.y;
		var xoffset=piece.x;
		
		for(var y=0;y<piece_matrix.length;y++) {
			for(var x=0;x<piece_matrix[y].length;x++) {
				var posy=yoffset+y;
				var posx=xoffset+x;
				if(piece_matrix[y][x] > 0) {
					currentPlayer.grid[posy][posx]=piece_matrix[y][x];
				}
			}
		}
		delete currentPlayer.currentPiece;
	}
	function swapPiece()
	{
		currentPlayer.unDrawPiece();
		if(currentPlayer.holdPiece) {
			var temp=currentPlayer.currentPiece.id;
			currentPlayer.currentPiece=loadPiece(currentPlayer.holdPiece);
			currentPlayer.holdPiece=temp;
			currentPlayer.drawPiece();
			currentPlayer.drawNext();
			currentPlayer.drawHold();
			send("PIECE");
			send("NEXT");
			send("HOLD");
		} else {
			currentPlayer.holdPiece=currentPlayer.currentPiece.id;
			getNewPiece();
			currentPlayer.drawHold();
			send("HOLD");
		}
	}
	function getNewPiece()
	{
		if(!currentPlayer.nextPiece) currentPlayer.nextPiece=random(7)+1;
		currentPlayer.currentPiece=loadPiece(currentPlayer.nextPiece);
		currentPlayer.nextPiece=random(7)+1;
		
		currentPlayer.drawPiece();
		currentPlayer.drawNext();
		currentPlayer.swapped=false;
		send("PIECE");
		send("NEXT");
	}
	function move(dir)
	{
		var piece=currentPlayer.currentPiece;
		
		var old_x=piece.x;
		var old_y=piece.y;
		
		var new_x=old_x;
		var new_y=old_y;
		
		switch(dir) 
		{
		case "DOWN":
			new_y=piece.y+1;
			break;
		case "LEFT":
			new_x=piece.x-1;
			break;
		case "RIGHT":
			new_x=piece.x+1;
			break;
		}
		
		piece.x=new_x;
		piece.y=new_y;

		if(checkInterference()) {
			piece.x=old_x;
			piece.y=old_y;
			return false;
		} 
		
		piece.x=old_x;
		piece.y=old_y;
		currentPlayer.unDrawPiece();
		piece.x=new_x;
		piece.y=new_y;
		currentPlayer.drawPiece();
		return true;
	}
	function rotate(dir)
	{
		var piece=currentPlayer.currentPiece;
		
		var old_o=piece.o;
		var new_o=old_o;
		var old_x=piece.x;
		var new_x=old_x;
		
		switch(dir)
		{
		case "RIGHT":
			new_o=piece.o-1;
			if(new_o<0) new_o=piece.orientation.length-1;
			break;
		case "LEFT":
			new_o=piece.o+1;
			if(new_o==piece.orientation.length) new_o=0;
			break;
		}
		var grid=piece.orientation[piece.o];
		var overlap=(piece.x + grid[0].length)-10;
		if(overlap>0) new_x=piece.x-overlap;
		
		piece.o=new_o;
		piece.x=new_x
	
		if(checkInterference()) {
			piece.x=old_x;
			piece.o=old_o;
			return false;
		} 
		
		piece.x=old_x;
		piece.o=old_o;
		currentPlayer.unDrawPiece();
		piece.x=new_x;
		piece.o=new_o;
		currentPlayer.drawPiece();
		return true;
	}
	function checkInterference()
	{
		var piece=currentPlayer.currentPiece;
		var piece_matrix=piece.orientation[piece.o];
		
		var yoffset=piece.y;
		var xoffset=piece.x;
		
		for(var y=0;y<piece_matrix.length;y++) {
			for(var x=0;x<piece_matrix[y].length;x++) {
				var posy=yoffset+y;
				var posx=xoffset+x;
				if(piece_matrix[y][x] > 0) {
					if(!currentPlayer.grid[posy] || currentPlayer.grid[posy][posx] == undefined) {
						return true;
					}
					if(currentPlayer.grid[posy][posx] > 0) {
						return true;
					}
				}
			}
		}
		return false;
	}
	function activePlayers()
	{
		var count=0;
		for each(var p in game.players) {
			if(p.active) count++;
		}
		return count;
	}
	function killPlayer(player)
	{	
		if(!game.players[player]) {
			log("player does not exist: " + player);
			return false;
		}
		game.players[player].active=false;
		game.players[player].clearBoard();
		display(game.players[player],"\1n\1r[\1hGAME OVER\1n\1r]");
		
		if(activePlayers()<=1) {
			for (var p in game.players) {
				if(game.players[p].active) game.winner=p;
			}
			endGame();
		}
	}
	function endGame()
	{
		game.status=2;
		storeGame(game);
		send("UPDATE");
	}
	function display(player,msg)
	{
		console.gotoxy(player.x+1,player.y+10);
		console.pushxy();
		message(msg);
	}
	function message(msg)
	{
		console.popxy();
		console.putmsg(centerString(msg,20));
		console.popxy();
		console.down();
		console.pushxy();
	}
	
	initGame();
	//initChat();
	main();
}

/*	data functions */
function loadPiece(index)
{
	var up;
	var down;
	var left;
	var right;
	var fg;
	var bg;
	
	switch(index) {
	case 1: // BAR 1 x 4 CYAN
		up=[
			[0,1,0,0],
			[0,1,0,0],
			[0,1,0,0],
			[0,1,0,0]
		];
		right=[
			[0,0,0,0],
			[1,1,1,1],
			[0,0,0,0],
			[0,0,0,0]
		];
 		down=up;
		left=right;
		fg=CYAN;
		bg=BG_CYAN;
		break;
	case 2: // BLOCK 2 x 2 LIGHTGRAY
		up=[
			[2,2],
			[2,2]
		];
		down=up;
		right=up;
		left=up;
		fg=LIGHTGRAY;
		bg=BG_LIGHTGRAY;
		break;
	case 3: // "T"	2 x 3 BROWN
		up=[
			[0,3,0],
			[3,3,3],
			[0,0,0]
		];
		right=[
			[0,3,0],
			[0,3,3],
			[0,3,0]
		];
		down=[
			[0,0,0],
			[3,3,3],
			[0,3,0]
		];
		left=[
			[0,3,0],
			[3,3,0],
			[0,3,0]
		];
		bg=BG_BROWN;
		fg=BROWN;
		break;
	case 4: // RH OFFSET 2 x 3 GREEN
		up=[
			[4,0,0],
			[4,4,0],
			[0,4,0]
		];
		right=[
			[0,4,4],
			[4,4,0],
			[0,0,0]
		];
		down=up;
		left=right;
		fg=GREEN;
		bg=BG_GREEN;
		break;
	case 5: // LH OFFSET 2 x 3 RED
		up=[
			[0,0,5],
			[0,5,5],
			[0,5,0]
		];
		right=[
			[5,5,0],
			[0,5,5],
			[0,0,0]
		];
		down=up;
		left=right;
		fg=RED;
		bg=BG_RED;
		break;
	case 6: // RH "L" 2 x 3 MAGENTA
		up=[
			[0,6,0],
			[0,6,0],
			[0,6,6]
		];
		right=[
			[0,0,0],
			[6,6,6],
			[6,0,0]
		];
		down=[
			[6,6,0],
			[0,6,0],
			[0,6,0]
		];
		left=[
			[0,0,6],
			[6,6,6],
			[0,0,0]
		];
		fg=MAGENTA;
		bg=BG_MAGENTA;
		break;
	case 7: // LH "L" 2 x 3 BLUE
		up=[
			[0,7,0],
			[0,7,0],
			[7,7,0]
		];
		right=[
			[7,0,0],
			[7,7,7],
			[0,0,0]
		];
		down=[
			[0,7,7],
			[0,7,0],
			[0,7,0]
		];
		left=[
			[0,0,0],
			[7,7,7],
			[0,0,7]
		];
		fg=BLUE;
		bg=BG_BLUE;
		break;
	}
	
	return new Piece(index,fg,bg,up,down,right,left);
}
function loadMini(index)
{
	var grid;
	var fg;
	var bg;
	
	switch(index) {
	case 1: // BAR 1 x 4 CYAN
		grid=[
			" \xDB ",
			" \xDB ",
			" \xDB "
		];
		fg=CYAN;
		bg=BG_CYAN;
		break;
	case 2: // BLOCK 2 x 2 LIGHTGRAY
		grid=[
			" \xDB\xDB",
			" \xDB\xDB",
			"   "
		];
		fg=LIGHTGRAY;
		bg=BG_LIGHTGRAY;
		break;
	case 3: // "T"	2 x 3 BROWN
		grid=[
			" \xDB ",
			"\xDB\xDB\xDB",
			"   "
		];
		bg=BG_BROWN;
		fg=BROWN;
		break;
	case 4: // RH OFFSET 2 x 3 GREEN
		grid=[
			" \xDB\xDB",
			"\xDB\xDB ",
			"   "
		];
		fg=GREEN;
		bg=BG_GREEN;
		break;
	case 5: // LH OFFSET 2 x 3 RED
		grid=[
			"\xDB\xDB ",
			" \xDB\xDB",
			"   "
		];
		fg=RED;
		bg=BG_RED;
		break;
	case 6: // RH "L" 2 x 3 MAGENTA
		grid=[
			" \xDB ",
			" \xDB ",
			" \xDB\xDB"
		];
		fg=MAGENTA;
		bg=BG_MAGENTA;
		break;
	case 7: // LH "L" 2 x 3 BLUE
		grid=[
			" \xDB ",
			" \xDB ",
			"\xDB\xDB "
		];
		fg=BLUE;
		bg=BG_BLUE;
		break;
	}
	
	return new Mini(fg,bg,grid);
}
function getFiles()
{
	stream.recvfile("tetris*.ini");
	stream.recvfile("players.ini",true);
}
function sendFiles(filename)
{
	stream.sendfile(filename);
}
function getAttr(index)
{
	switch(Number(index))
	{
	case 1:
		console.attributes=CYAN;
		break;
	case 2:
		console.attributes=LIGHTGRAY;
		break;
	case 3:
		console.attributes=BROWN;
		break;
	case 4:
		console.attributes=GREEN;
		break;
	case 5:
		console.attributes=RED;
		break;
	case 6:
		console.attributes=MAGENTA;
		break;
	case 7:
		console.attributes=BLUE;
		break;
	}
}
function getMatrix(h,w)
{
	var grid=new Array(h);
	for(var y=0;y<grid.length;y++) {
		grid[y]=getNewLine(w);
	}
	return grid;
}
function getGarbageLine(space)
{
	var line=new Array(10);
	for(var x=0;x<line.length;x++) {
		if(x==space) line[x]=0;
		else {
			line[x]=random(7)+1;
		}
	}
	return line;
}
function getNewLine(w)
{
	var line=new Array(w);
	for(var x=0;x<line.length;x++) {
		line[x]=0;
	}
	return line;
}
function storeGame(game)
{
	//STORE GAME DATA
	var file=new File(game.dataFile+".tmp");
	file.open("w+");
	
	file.iniSetValue(null,"gameNumber",game.gameNumber);
	file.iniSetValue(null,"status",game.status);
	file.iniSetValue(null,"winner",game.winner);
	if(game.created) file.iniSetValue(null,"created",game.created);
	
	for(var p in game.players) {
		file.iniSetValue("players",p,game.players[p].active);
	}

	file.close();
	file_remove(game.dataFile);
	file_rename(file.name,game.dataFile);
	sendFiles(game.dataFile);
}
function storePlayerData(game,id)
{
	var file=new File(game.dataFile);
	file.open('r+',true);
	file.iniSetValue("players",id,game.players[id].active);
	file.close();
	sendFiles(game.dataFile);
}
function notify(message)
{
	chat.chatroom.alert(message);
}
function notice(message)
{
	chat.chatroom.notice(message);
}
function help()
{
	//TODO: write help file
}


splashStart();
lobby();
splashExit();
