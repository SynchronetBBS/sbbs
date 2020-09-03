/*
	JAVASCRIPT INTER-BBS SEA BATTLE (Battleship)
	For Synchronet v3.15+
	Matt Johnson(2009)
*/
//$Id: game.js,v 1.1 2010/10/01 14:31:59 mcmlxxix Exp $
load("chateng.js");
//load("helpfile.js"); //TODO: write instructions!

var root=js.exec_dir;

load(root + "battle.js");
load(root + "menu.js");

var stream=argv[0];
var players=new PlayerList();
var chat=new ChatEngine(root);
var oldpass=console.ctrl_key_passthru;

function lobby()
{
	var table_graphic=new Graphic(10,5);
	table_graphic.load(root+"bsmall.bin");
	var lobby_graphic=new Graphic(80,24);
	lobby_graphic.load(root+"lobby.bin");
	var clearinput=true;
	var scrollBar=new Scrollbar(3,24,37,"horizontal","\1y");
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
		console.clear(ANSI_NORMAL);
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
		processGraphic();
		initChat();
		initMenu();
	}
	function splashExit()
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		console.ctrlkey_passthru=oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		console.attributes=ANSI_NORMAL;
		console.clear();
		
		var splash_filename=root + "exit.bin";
		if(!file_exists(splash_filename)) return;
		
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
	}
	function initChat()
	{
		chat.init(38,19,42,3);
		chat.input_line.init(42,23,38,"","\1n");
		chat.joinChan("sea-battle lobby",user.alias,user.name);
	}
	function initMenu()
	{
		menu=new Menu(		"\1n","\1c\1h");
		var menu_items=[	"~Select Game"					, 
							"~New Game"						,
							"~Rankings"						,
							"~Help"							,
							"Re~draw"						];
		menu.add(menu_items);
	}
	function processGraphic()
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
	function updateTables()
	{
		var update=false;
		var game_files=directory(root+"*.bom");
		for(i in game_files) {
			var filename=file_getname(game_files[i]);
			var gameNumber=parseInt(filename.substring(0,filename.indexOf(".")),10);
			if(tables[gameNumber]) {
				var lastupdated=file_date(game_files[i]);
				var lastloaded=tables[gameNumber].lastupdated;
				if(lastupdated>lastloaded) {
					loadTable(game_files[i]);
					update=true;
				}
			} else {
				loadTable(game_files[i]);
				update=true;
			}
		}
		for(t in tables) {
			var table=tables[t];
			if(table && !file_exists(table.gamefile.name)) {
				delete tables[t];
				update=true;
			}
		}
		return update;
	}
	function loadTable(gamefile,index)
	{
		var table=new GameData(gamefile);
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
			clearBlock(posx,posy,19,10);
			
			var pointer=pointers[index];
			if(tables[pointer])
			{
				var tab=tables[pointer];
				
				console.gotoxy(posx+11,posy);
				console.putmsg("\1n\1yTABLE: \1h" + tab.gameNumber);
				
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
					console.putmsg(printPadded(players.formatPlayer(tab.players[player].id,turn),19));
					console.gotoxy(x+1,y+1);
					console.putmsg(players.formatStats(tab.players[player].id));
					y+=2;
				}
				table_graphic.draw(posx,posy);
				index++;
			}
		}
	}
	function drawLobby()
	{
		lobby_graphic.draw();
		write(console.ansi(BG_BLACK));
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		updateTables();
		drawLobby();
		drawTables();
		chat.redraw();
	}


/*	MAIN FUNCTIONS	*/
	function main()
	{
		redraw();
		while(1)
		{
			if(updateTables()) drawTables();
			chat.cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
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
					if((scroll_index+1)<=range && tables.length>4)	{
						scroll_index+=2;
						drawTables();
					}
					break;
				case "/":
					if(!chat.input_line.buffer.length)	{
						refreshCommands();
						listCommands();
						lobbyMenu();
						redraw();
					}
					else if(!Chat(k,chat)) return;
					break;
				case "\x1b":	
					return;
					break;
				default:
					if(!Chat(k,chat)) return;
					break;
				}
			}
		}
	}
	function lobbyMenu()
	{
		refreshCommands();
		listCommands();
		menuPrompt("\1nMake a selection: ");
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		if(menu.items[k] && menu.items[k].enabled) 
			switch(k.toUpperCase())
			{
			case "N":
				if(startNewGame()) initChat();
				break;
			case "R":
				showRankings();
				break;
			case "H":
				//gamehelp.help("lobby");
				break;
			case "S":
				if(selectGame()) initChat();
				break;
			default:
				break;
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
				chat.notify("\1nPassword: ");
				var password=console.getstr(25);
				if(password!=tables[num].password) return false;
			}
			playGame(tables[num]);
			return true;
		}
		else return false;
	}
	function startNewGame()
	{
		var newgame=new GameData();
		clearinput=true;

		menuPrompt("\1nSingle Player Game? [\1hy\1n,\1hN\1n]: ");
		var single=console.getkeys("\x1bYN");
		switch(single)
		{
			case "Y":
				newgame.singleplayer=true;
				break;
			case "\x1b":
				notice("\1r\1hGame Creation Aborted");
				return false;
			default:
				newgame.singleplayer=false;
				break;
		}
		
		if(!newgame.singleplayer)
		{
			menuPrompt("\1nPrivate game? [\1hy\1n,\1hN\1n]: ");
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
				case "\x1b":
					notice("\1r\1hGame Creation Aborted");
					return false;
				default:
					newgame.password=false;
					break;
			}
		}

		menuPrompt("\1nAttacks equal to # of ships? [\1hy\1n,\1hN\1n]: ");
		var multi=console.getkeys("\x1bYN");
		switch(multi)
		{
			case "Y":
				newgame.multishot=true;
				break;
			case "\x1b":
				notice("\1r\1hGame Creation Aborted");
				return false;
			default:
				newgame.multishot=false;
				break;
		}
		
		menuPrompt("\1nBonus attack for each hit? [\1hy\1n,\1hN\1n]: ");
		var bonus=console.getkeys("\x1bYN");
		switch(bonus)
		{
			case "Y":
				newgame.bonusattack=true;
				break;
			case "\x1b":
				notice("\1r\1hGame Creation Aborted");
				return false;
			default:
				newgame.bonusattack=false;
				break;
		}
		
		menuPrompt("\1nAllow spectators to see board? [\1hy\1n,\1hN\1n]: ");
		var spectate=console.getkeys("\x1bYN");
		switch(spectate)
		{
			case "Y":
				newgame.spectate=true;
				break;
			case "\x1b":
				notice("\1r\1hGame Creation Aborted");
				return false;
			default:
				newgame.spectate=false;
				break;
		}
		playGame(newgame,true);
		return true;
	}

	splashStart();
	main();
	splashExit();
}
function playGame(game,join) 
{
	var clearinput=true;
	var currentplayer;
	var shots;
	var menu;
	var name;
	var infobar=true; //TOGGLE FOR GAME INFORMATION BAR AT TOP OF CHAT WINDOW (default: false)
	
	function initGame()
	{
		game.loadGame();
		game.loadPlayers();
		if(join) {
			game.turn=players.getFullName(user.alias);
			game.storeGame();
			if(game.singleplayer) computerInit();
			joinGame();
		}
		else currentplayer=game.findCurrentUser();
		if(!game.started) game.startGame();
	}
	function initChat()
	{
		chat.init(35,16,45,6);
		chat.input_line.init(45,23,35,"","\1n");
		chat.partChan("sea-battle lobby",user.alias);
		chat.joinChan("sea-battle table " + game.gameNumber,user.alias,user.name);
	}
	function initMenu()
	{
		menu=new Menu(		"\1n","\1c\1h");
		var menu_items=[	"~Sit"							, 
							"~New Game"						,
							"~Place Ships"					,
							"~Attack"						,
							"~forfeit"						,
							"~Help"							,
							"Game ~Info"					,
							"Re~draw"						];
		menu.add(menu_items);
		refreshCommands();
	}
	function main()
	{
		redraw();
		updateStatus();
		while(1) {
			cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k) {
				if(clearinput) {
					chat.input_line.clear();
					clearinput=false;
				} 
				switch(k) {
					case KEY_UP:
					case KEY_DOWN:
					case KEY_LEFT:
					case KEY_RIGHT:
						if(game.turn==currentplayer.id && !game.finished)
							attack();
						else if(!Chat(k,chat) && handleExit()) {
							chat.partChan("sea-battle table " + game.gameNumber,user.alias);
							return true;
						}
						break;
					case "/":
						if(!chat.input_line.buffer.length) {
							gameMenu();
						}
						else if(!Chat(k,chat) && handleExit()) { 
							chat.partChan("sea-battle table " + game.gameNumber,user.alias);
							return true;
						}
						break;
					case "\x1b":	
						if(handleExit()) {
							chat.partChan("sea-battle table " + game.gameNumber,user.alias);
							return true;
						}
						break;
					default:
						if(!Chat(k,chat) && handleExit()) {
							chat.partChan("sea-battle table " + game.gameNumber,user.alias);
							return true;
						}
						break;
				}
			}
		}
	}
	function updateStatus()
	{
		if(game.finished) {
			notice("\1n\1c Game completed");
			notice("\1n\1c Winner\1h: " + players.getAlias(game.winner));
			
		} else if(game.started) {
			notice("\1n\1c Game in progress");
			if(currentplayer && game.turn==currentplayer.id) 
				notice("\1c\1h It's your turn");
			else 
				notice("\1n\1c It's " + players.getAlias(game.turn) + "'s turn");
		} else {
			if(game.players.length <2) {
				notice("\1n\1c Waiting for more players");
			}
			for(p in game.players)	{
				var player=game.players[p];
				if(!player.ready) {
					if(currentplayer && currentplayer.id==player.id) {
						notice("\1n\1c You must place your ships");
					} else {
						notice("\1n\1c " + player.name + " is not ready");
					}
				} else {
					if(currentplayer && currentplayer.id==player.id) {
						notice("\1n\1c You are ready");
					} else	{
						notice("\1n\1c " + player.name + " is ready");
					}
				}
			}
		}
		chat.redraw();
	}
	function handleExit()
	{
		if(game.finished && currentplayer)
		{
			if(currentplayer.id!=game.winner || game.singleplayer)	
			{
				if(file_exists(game.gamefile.name))
					file_remove(game.gamefile.name);
				if(file_exists(root + name + ".his"))
					file_remove(root + name + ".his");
			}
		}
		return true;
	}
	function refreshCommands()
	{
		if(!currentplayer || currentplayer.ready) menu.disable(["P"]);
		else if(game.started || game.finished) menu.disable(["P"]);
		else menu.enable(["P"]);
		if(currentplayer) menu.disable(["S"]);
		else if(game.players.length==2) menu.disable(["S"]);
		else menu.enable(["S"]);
		if(!game.finished || !currentplayer) menu.disable(["N"]);
		else menu.enable(["N"]);
		if(!game.started || game.turn!=currentplayer.id || game.finished) 
		{
			menu.disable(["A"]);
			menu.disable(["F"]);
		}
		else 
		{
			menu.enable(["A"]);
			menu.enable(["F"]);
		}
	}
	function gameMenu()
	{	
		refreshCommands();
		listCommands();
		menuPrompt("\1nMake a selection: ");
		
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		clearAlert();
		if(k) {
			chat.chatroom.clear();
			chat.redraw();
			if(menu.items[k] && menu.items[k].enabled) {
				switch(k)
				{
				case "S":
					joinGame();
					break;
				case "H":
					//gamehelp.help("gameplay");
					break;
				case "D":
					redraw();
					break;
				case "F":
					forfeit();
					break;
				case "I":
					updateStatus();
					break;
				case "P":
					placeShips();
					break;
				case "A":
					attack();
					break;
				case "N":
					Rematch();
					break;
				default:
					break;
				}
				chat.chatroom.clear();
				chat.redraw();
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
	function infoBar()
	{
		console.gotoxy(chat.chatroom.x,1);
		console.putmsg("\1k\1h[\1cEsc\1k\1h]\1n return to lobby \1k\1h[\1c/\1k\1h]\1n menu");
		if(infobar) {
			var x=chat.chatroom.x;
			var y=3;
			var turn=false;
			if(game.started && !game.finished) turn=game.turn;
			for(player in game.players)
			{
				console.gotoxy(x,y);
				console.putmsg(printPadded(players.formatPlayer(game.players[player].id,turn),27));
				console.putmsg("\1nShips\1h: \1n" + countShips(game.players[player].id));
				y+=1;
			}
		}
	}
	function clearAlert()
	{
		chat.input_line.clear();
	}
	function processData(packet)
	{
		debug(packet);
		if(packet.gameNumber != game.gameNumber) return false;
		switch(packet.func.toUpperCase())
		{
			case "ATTACK":
				getAttack(packet);
				infoBar();
				break;
			case "ENDTURN":
				game.nextTurn();
				if(currentplayer && game.turn == currentplayer.id) 
					notice("\1c\1hIt's your turn");
				infoBar();
				break;
			case "ENDGAME":
				var str1=players.getAlias(packet.winner);
				var str2="";
				if(currentplayer && packet.loser==currentplayer.id) str2="your";
				else str2=players.getAlias(packet.loser) + "'s";
				for(player in game.players) {
					game.players[player].board.hidden=false;
					game.players[player].board.drawBoard();
				}
				notice("\1r\1h" + str1 + " sank all of " + str2 + " ships!");
				game.finished=true;
				game.winner=packet.winner;
				updateStatus();
				infoBar();
				break;
			case "UPDATE":
				game.loadGame();
				game.loadPlayers();
				if(!currentplayer && game.spectate && game.started) {
					for(player in game.players) {
						game.players[player].board.hidden=false;
						game.players[player].board.drawBoard();
					}
				}
				game.startGame();
				updateStatus();
				infoBar();
				break;
			default:
				break;
		}
	}
	function cycle()
	{
		chat.cycle();
		var packet=stream.receive();
		if(packet)	processData(packet);
	}
	function redraw()
	{
		console.clear();
		game.redraw();
		infoBar();
		chat.redraw();
		drawLine(44,2,37,DARKGRAY);
		drawLine(44,5,37,DARKGRAY);
		drawLine(44,22,37,DARKGRAY);
		drawLine(44,24,36,DARKGRAY);
	}
	function packageData(data,func)
	{
		switch(func.toUpperCase())
		{
			case "ALERT":
				data.msg=data;
				break;
			case "ATTACK":
			case "ENDTURN":
			case "ENDGAME":
			case "UPDATE":
				break;
			default:
				debug("DATA: " + data.toSource());
				debug("FUNC: " + func);
				log("Unknown sea-battle data not sent");
				break;
		}
		data.func=func;
		data.gameNumber=game.gameNumber;
		return data;
	}
	function send(data,func)
	{
		var d=packageData(data,func);
		stream.send(d);
	}
	function clearAlert()
	{
		chat.input_line.clear();
	}
	function menuPrompt(msg)
	{
		console.popxy();
		console.down();
		console.pushxy();
		console.putmsg(msg);
	}

/*	ATTACK FUNCTIONS	*/
	function attack()
	{
		var player=currentplayer;
		var opponent=game.findOpponent();
		if(!shots) shots=(game.multishot?countShips(player.id):1);
		while(shots>0) {
			cycle();
			var coords=chooseTarget(opponent);
			if(!coords) return false;
			opponent.board.shots[coords.x][coords.y]=true;
			opponent.board.drawSpace(coords.x,coords.y);
			sendAttack(coords,opponent);
			game.storeShot(coords,opponent.id);
			var hit=checkShot(coords,opponent);
			if(game.bonusattack) {
				if(!hit) shots--;
			} else {
				shots--;
			}
			var count=countShips(opponent.id);
			if(count===0) {
				var p1=game.findCurrentUser();
				var p2=game.findOpponent();
				endGame(p1,p2);
				break;
			}
			infoBar();
		}
		shots=false;
		game.storeGame();
		if(!game.finished) {
			game.nextTurn();
			if(game.turn==players.getFullName("CPU") && !game.finished) computerTurn();
			else game.notifyPlayer();
			send(new Packet("ENDTURN"),"ENDTURN");
		}
		infoBar();
		return true;
	}
	function chooseTarget(opponent)
	{
		var letters="abcdefghij";
		var selected;
		var board=opponent.board;
		selected={"x":0,"y":0};
		board.drawSelected(selected.x,selected.y);
		notify("\1nChoose target (\1hshots\1n: \1h" + shots + "\1n)");
		while(1) {
			cycle();
			var key=console.inkey(K_NOECHO|K_NOCRLF|K_UPPER,50);
			if(key) {
				if(key=="\r") {
					if(!board.shots[selected.x][selected.y]) {
						board.unDrawSelected(selected.x,selected.y);
						return selected;
					} else {
						notify("\1r\1hInvalid Selection");
					}
				}
				switch(key.toLowerCase())
				{
					case KEY_DOWN:
						board.unDrawSelected(selected.x,selected.y);
						if(selected.y<9) selected.y++;
						else selected.y=0;
						break;
					case KEY_UP:
						board.unDrawSelected(selected.x,selected.y);
						if(selected.y>0) selected.y--;
						else selected.y=9;
						break;
					case KEY_LEFT:
						board.unDrawSelected(selected.x,selected.y);
						if(selected.x>0) selected.x--;
						else selected.x=9;
						break;
					case KEY_RIGHT:
						board.unDrawSelected(selected.x,selected.y);
						if(selected.x<9) selected.x++;
						else selected.x=0;
						break;
					case "0":
						key=10;
					case "1":
					case "2":
					case "3":
					case "4":
					case "5":
					case "6":
					case "7":
					case "8":
					case "9":
						key-=1;
						board.unDrawSelected(selected.x,selected.y);
						selected.x=key;
						break;
					case "a":
					case "b":
					case "c":
					case "d":
					case "e":
					case "f":
					case "g":
					case "h":
					case "i":
					case "j":
						board.unDrawSelected(selected.x,selected.y);
						selected.y=letters.indexOf(key.toLowerCase());
						break;
					case "\x1B":
						board.unDrawSelected(selected.x,selected.y);
						return false;
					default:
						continue;
				}
				board.drawSelected(selected.x,selected.y);
			}
		}
		
	}
	function sendAttack(coords,opponent)
	{
		var attack=new Attack(currentplayer.id,opponent.id,coords);
		send(attack,"ATTACK");
	}
	function checkShot(coords,opponent)
	{
		if(opponent.board.grid[coords.x][coords.y])
		{
			var segment=opponent.board.grid[coords.x][coords.y].segment;
			var shipid=opponent.board.grid[coords.x][coords.y].id;
			var ship=opponent.board.ships[shipid];
			ship.takeHit(segment);
			if(ship.sunk)
			{
				notice("\1r\1hYou sank " + opponent.name + "'s ship!");
			}
			else
			{
				notice("\1r\1hYou hit " + opponent.name + "'s ship!");
			}
			return true;
		}
		else
		{
			notice("\1c\1hYou missed!");
			return false;
		}
	}
	function getAttack(attack)
	{
		var attacker=game.findPlayer(attack.attacker);
		var defender=game.findPlayer(attack.defender);
		var coords=attack.coords;
		var board=defender.board;
		var targetname=defender.name + "'s";
		if(currentplayer && defender.id==currentplayer.id) targetname="your";
		if(board.grid[coords.x][coords.y])
		{
			var shipid=board.grid[coords.x][coords.y].id;
			var segment=board.grid[coords.x][coords.y].segment;
			board.ships[shipid].takeHit(segment);
			if(board.ships[shipid].sunk)
			{
				notice("\1r\1h" + attacker.name  + " sank " + targetname + " ship!");
			}
			else
			{
				notice("\1r\1h" + attacker.name  + " hit " + targetname + " ship!");
			}
		}
		else
		{
			notice("\1c\1h" + attacker.name  + " fired and missed!");
		}
		board.shots[coords.x][coords.y]=true;
		board.drawSpace(coords.x,coords.y);
	}
	function countShips(id)
	{
		var player=game.findPlayer(id);
		var board=player.board;
		var sunk=0;
		for(ship in board.ships)
		{
			if(board.ships[ship].sunk) sunk++;
		}
		return(board.ships.length-sunk);
	}

/*	GAMEPLAY FUNCTIONS	*/
	function checkInterference(position,ship,board)
	{
		var segments=[];
		var xinc=(ship.orientation=="vertical"?0:1);
		var yinc=(ship.orientation=="vertical"?1:0);

		for(segment=0;segment<ship.segments.length;segment++)
		{
			var posx=(segment*xinc)+position.x;
			var posy=(segment*yinc)+position.y;
			if(board.grid[posx][posy])
			{
				segments.push({'x':posx,'y':posy});
			}
		}

		if(segments.length) return segments;
		return false;
	}
	function placeShips()
	{	
		var board=currentplayer.board;
		notify("\1n[\1hspace\1n] rotate [\1henter\1n] place");
		for(id in board.ships) {
			cycle();
			var ship=board.ships[id];
			while(1) {
				var position=selectTile(ship);
				if(position) 
				{
					board.addShip(position,id);
					break;
				}
				//else return false;
			}
		}
		currentplayer.ready=true;
		game.storeGame();
		game.storeBoard(currentplayer.id);
		game.storePlayer(currentplayer.id);
		game.startGame();
		updateStatus();
		infoBar();
		send(new Packet("UPDATE"),"UPDATE");
	}
	function selectTile(ship,placeholder)
	{
		var selected;
		var board=currentplayer.board;
		if(placeholder) selected={"x":start.x, "y":start.y};
		else {
			selected={"x":0,"y":0};
		}
		board.drawShip(selected.x,selected.y,ship);
		var interference=checkInterference(selected,ship,board);
		var ylimit=(ship.orientation=="vertical"?ship.segments.length-1:0);
		var xlimit=(ship.orientation=="horizontal"?ship.segments.length-1:0);
		while(1) {
			cycle();
			var key=console.inkey(K_NOECHO|K_NOCRLF|K_UPPER,50);
			if(key)	{
				if(key=="\r") {
					if(!interference) return selected;
					else {
						notify("\1r\1hInvalid Selection");
					}
				}
				switch(key)
				{
				case KEY_DOWN:
				case "2":
					if(selected.y + ylimit<9)	{
						board.unDrawShip(selected.x,selected.y,ship);
						selected.y++;
					}
					break;
				case KEY_UP:
				case "8":
					if(selected.y>0) {
						board.unDrawShip(selected.x,selected.y,ship);
						selected.y--;
					}
					break;
				case KEY_LEFT:
				case "4":
					if(selected.x>0) {
						board.unDrawShip(selected.x,selected.y,ship);
						selected.x--;
					}
					break;
				case KEY_RIGHT:
				case "6":
					if(selected.x + xlimit<9) {
						board.unDrawShip(selected.x,selected.y,ship);
						selected.x++;
					}
					break;
				case "7":
					if(selected.x>0 && selected.y>0) {
						board.unDrawShip(selected.x,selected.y,ship);
						selected.x--;
						selected.y--;
					}
					break;
				case "9":
					if(selected.x + xlimit<9 && selected.y>0)	{
						board.unDrawShip(selected.x,selected.y,ship);
						selected.x++;
						selected.y--;
					}
					break;
				case "1":
					if(selected.x>0 && selected.y + ylimit<9)	{
						board.unDrawShip(selected.x,selected.y,ship);
						selected.x--;
						selected.y++;
					}
					break;
				case "3":
					if(selected.x + xlimit<9 && selected.y + ylimit<9) {
						board.unDrawShip(selected.x,selected.y,ship);
						selected.x++;
						selected.y++;
					}
					break;
				case " ":
					board.unDrawShip(selected.x,selected.y,ship);
					ship.toggle();
					ylimit=(ship.orientation=="vertical"?ship.segments.length-1:0);
					xlimit=(ship.orientation=="horizontal"?ship.segments.length-1:0);
					switch(ship.orientation)
					{
					case "horizontal":
						while(selected.x + xlimit>9) selected.x--;
						break;
					case "vertical":
						while(selected.y + ylimit>9) selected.y--;
						break;
					}
					break;
				case "\x1B":
					board.unDrawShip(selected.x,selected.y,ship);
					return false;
				default:
					continue;
				}
				if(interference) {
					for(pos in interference) {
						board.drawSpace(interference[pos].x,interference[pos].y);
					}
				}
				board.drawShip(selected.x,selected.y,ship);
				interference=checkInterference(selected,ship,board);
			}
		}
	}
	function newGame()
	{
		//TODO: Add the ability to change game settings (timed? rated?) when restarting/rematching
		game.newGame();
		send(new Packet("UPDATE"),"UPDATE");
	}
	function forfeit()
	{
		notify("\1c\1hforfeit this game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
		var p1=game.findCurrentUser();
		var p2=game.findOpponent();
		endGame(p2,p1);
	}
	function endGame(winner,loser)
	{
		var wdata=players.players[winner.id];
		var ldata=players.players[loser.id];
		
		if(winner.name!="CPU") {
			wdata.wins++;
			players.storePlayer(winner.id);
			if(winner.id==currentplayer.id)	{
				notice("\1r\1hYou sank all of " + loser.name + "'s ships!");
			}
		}
		if(loser.name!="CPU") {
			ldata.losses++;
			players.storePlayer(loser.id);
			game.notifyLoser(loser.id);
		}
		game.winner=winner.id;
		game.end();
		var endgame={'winner':winner.id, 'loser':loser.id};
		send(endgame,"ENDGAME");
		infoBar();
	}
	function joinGame()
	{
		game.addPlayer(user.alias);
		currentplayer=game.findCurrentUser();
		send(new Packet("UPDATE"),"UPDATE");
		infoBar();
	}

/* AI FUNCTIONS	*/
	function computerTurn()
	{
		var cpu=game.findComputer();
		var opponent=currentplayer;
		var shots=(game.multishot?countShips(cpu.id):1);
		while(shots>0)
		{
			var coords=computerAttack(opponent);
			opponent.board.shots[coords.x][coords.y]=true;
			sendAttack(coords,opponent);
			game.storeShot(coords,opponent.id);
			var hit=checkComputerAttack(coords,opponent);
			if(game.bonusattack) 
			{
				if(!hit) shots--;
			}
			else shots--;
			opponent.board.drawSpace(coords.x,coords.y);
			var count=countShips(opponent.id);
			if(count===0) 
			{
				var winner=cpu;
				var loser=opponent;
				endGame(winner,loser);
				break;
			}
		}
		game.nextTurn();
		game.storeGame();
		send(new Packet("ENDTURN"),"ENDTURN");
		return true;
	}
	function checkComputerAttack(coords,opponent)
	{
		if(opponent.board.grid[coords.x][coords.y])
		{
			var segment=opponent.board.grid[coords.x][coords.y].segment;
			var shipid=opponent.board.grid[coords.x][coords.y].id;
			var ship=opponent.board.ships[shipid];
			game.lastcpuhit=coords;
			ship.takeHit(segment);
			if(ship.sunk)
			{
				notice("\1r\1hCPU sank your ship!");
			}
			else notice("\1r\1hCPU hit your ship!");
			return true;
		}
		else
		{
			notice("\1c\1hCPU fired and missed!");
			return false;
		}
	}
	function computerAttack(opponent)
	{
		var coords=new Object();
		if(game.lastcpuhit && opponent.board.grid[game.lastcpuhit.x][game.lastcpuhit.y])
		{
			var shipid=opponent.board.grid[game.lastcpuhit.x][game.lastcpuhit.y].id;
			if(!opponent.board.ships[shipid].sunk)
			{
				var start={'x':game.lastcpuhit.x,'y':game.lastcpuhit.y};
				coords=findNearbyTarget(opponent.board,start);
				if(coords) return coords;
			}
		}
		else
		{
			for(x in opponent.board.shots)
			{
				for(y in opponent.board.shots[x])
				{
					if(opponent.board.grid[x][y])
					{
						var shipid=opponent.board.grid[x][y].id;
						if(!opponent.board.ships[shipid].sunk)
						{
							var start={'x':x,'y':y};
							coords=findNearbyTarget(opponent.board,start);
							if(coords) return coords;
						}
					}
				}
			}
		}
		coords=findRandomTarget(opponent.board);
		return(coords);
	}
	function findNearbyTarget(board,coords)
	{
		var tried=[];
		var tries=4;
		while(tries>0)
		{
			var randdir=random(4);
			if(!tried[randdir])
			{
				switch(randdir)
				{
					case 0: //north
						if(coords.y>0 && !board.shots[coords.x][coords.y-1]) 
						{
							coords.y--;
							return coords;
						}
						break;
					case 1: //south
						if(coords.y<9 && !board.shots[coords.x][coords.y+1]) 
						{
							coords.y++;
							return coords;
						}
						break;
					case 2: //east
						if(coords.x<9 && !board.shots[coords.x+1][coords.y]) 
						{
							coords.x++;
							return coords;
						}
						break;
					case 3: //west
						if(coords.x>0 && !board.shots[coords.x-1][coords.y]) 
						{
							coords.x--;
							return coords;
						}
						break;
				}
				tried[randdir]=true;
				tries--;
			}
		}
		return false;
	}
	function findRandomTarget(board)
	{
		while(1)
		{
			column=random(10);
			row=random(10);
			if(!board.shots[column][row]) break;
		}
		return({'x':column,'y':row});
	}
	function computerPlacement()
	{
		var cpu=game.findComputer();
		var board=cpu.board;
		for(s in board.ships)
		{
			var ship=board.ships[s];
			var randcolumn=random(10);
			var randrow=random(10);
			var randorient=random(2);
			var orientation=(randorient==1?"horizontal":"vertical");
			var position={'x':randcolumn, 'y':randrow};
			ship.orientation=orientation;
			while(1)
			{
				var xlimit=(orientation=="horizontal"?ship.segments.length:0);
				var ylimit=(orientation=="vertical"?ship.segments.length:0);
				if(position.x + xlimit <=9 && position.y + ylimit<=9)
				{
					if(!checkInterference(position,ship,board))
					{
						break;
					}
				}
				randcolumn=random(10);
				randrow=random(10);
				randorient=random(2);
				orientation=(randorient==1?"horizontal":"vertical");
				position={'x':randcolumn, 'y':randrow};
				ship.orientation=orientation;
			}
			board.addShip(position,s);
		}
		cpu.ready=true;
		game.storeBoard(cpu.id);
		game.storePlayer(cpu.id);
		send(new Packet("UPDATE"),"UPDATE");
	}
	function computerInit()
	{
		game.addPlayer("CPU");
		computerPlacement();
	}
	
	initGame();
	initChat();
	initMenu();
	main();
}

function notice(msg)
{
	chat.chatroom.notice(msg);
}
function notify(msg)
{
	chat.chatroom.alert(msg);
	clearinput=true;
}
function sendFiles(filename)
{
	stream.sendfile(filename);
}
function getBoardPosition(position)
{
	var letters="abcdefghij";
	if(typeof position=="string") {
		//CONVERT BOARD POSITION IDENTIFIER TO XY COORDINATES
		var x=position.charAt(1);
		var y=position.charAt(0);
		x=parseInt(x==0?9:position.charAt(1)-1,10);
		y=parseInt(letters.indexOf(y.toLowerCase()),10);
		position={'x':x,'y':y};
	} else if(typeof position=="object") {
		//CONVERT XY COORDINATES TO STANDARD CHESS BOARD FORMAT
		var x=(position.x==9?0:parseInt(position.x,10)+1);
		position=(letters.charAt(position.y) + x);
	}
	return(position);
}

lobby();