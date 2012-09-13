load("sbbsdefs.js");
load("inputline.js");
load("layout.js");
load("json-chat.js");

load(root+"diceobj.js");
load(root+"dicefunc.js");

var frame = new Frame();
var inputFrame = new Frame(10,24,70,1,BG_LIGHTGRAY|BLACK,frame);
var promptFrame = new Frame(10,24,70,1,BG_BROWN|WHITE,frame);
var input = new InputLine(inputFrame);
var	chat = new JSONChat(user.number,undefined,serverAddr,serverPort);

function open() {
	console.clear();
	console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
	bbs.sys_status|=SS_MOFF;
	bbs.sys_status|=SS_PAUSEOFF;
	splash("dicewarz2.bin");
}
function close() {
	console.ctrlkey_passthru=oldpass;
	bbs.sys_status&=~SS_MOFF;
	bbs.sys_status&=~SS_PAUSEOFF;
	splash("exit.bin");
}
function loadGraphic(filename) {
	var f=new Frame(1,1,80,24,undefined,frame);
	f.load(filename,80,24);
	f.open();
	return f;
}
function menuPrompt(string,append,keepOpen,hotkeys) {
	if(!append) 
		promptFrame.clear();
	if(!promptFrame.is_open) 
		promptFrame.open();
	promptFrame.putmsg(string);
	promptFrame.draw();
	
	var cmd="";
	while(!js.terminated) {
		var k = console.getkey(K_NOECHO|K_NOCRLF|K_UPPER|K_NOSPIN);
		if(hotkeys) 
			return k;
		if(k == "\r")
			break;
		cmd+=k;
		promptFrame.putmsg(k);
		promptFrame.cycle();
	}
	if(!keepOpen)
		promptFrame.close();
	return cmd;
}
function menuText(string,append) {
	if(!append) 
		promptFrame.clear();
	if(!promptFrame.is_open) 
		promptFrame.open();
	promptFrame.putmsg(string);
	promptFrame.draw();
}
function splash(file) {
	if(file_exists(root + file)) {
		var splash=new Frame(1,1,80,24,undefined,frame);
		splash.load(root + file,80,24);
		splash.gotoxy(1,24);
		splash.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		splash.draw();
		console.getkey(K_NOECHO|K_NOSPIN);
		splash.delete();
	}
}
function submitMessage(f,msg) {
	var str = "";
	if(msg.nick)
		var str = getColor(chat.settings.NICK_COLOR) + msg.nick.name + "\1n: " + 
		getColor(chat.settings.TEXT_COLOR) + msg.str;
	else
		var str = getColor(chat.settings.NOTICE_COLOR) + msg.str;
	f.putmsg(str + "\r\n");
}

/* main lobby */
function lobby() {
	var layout=new Layout(frame);
	var lobbyBackground=loadGraphic(root+settings.lobby_file);
	var listFrame=new Frame(2,2,78,13,undefined,frame);
	var menuFrame=new Frame(10,24,70,1,BG_BROWN|BLACK,frame);
	var chatFrame=new Frame(2,16,78,8,BG_BLACK|LIGHTGRAY,frame);
	var scoreFrame=new Frame(10,5,60,15,BG_BLUE|YELLOW,frame);
	var channel="#dicewarz2";
	var menu,menuFrame;

	function open() {
		client.subscribe(game_id,"games");
		chat.join(channel);
		
		var menuItems=[	
			"~Rankings", 
			"~Select game",
			"~Begin new game",
			"~Help",
			"\~Chat",
			"~Quit"
		];
		menu=new Menu(menuItems,menuFrame,"\1w\1h","\1n\1k");

		chatFrame.lf_strict=false;
		
		inputFrame.open();
		menuFrame.open();
		listFrame.open();
		chatFrame.open();
		menu.draw();
		gameList();
	}	
	function main() {
		var hotkeys=true;
		while(1) {
			cycle();
			var cmd=input.getkey(hotkeys);
			if(!cmd) 
				continue;
			if(hotkeys) {
				//menu.clear();
				switch(cmd.toUpperCase())	{
				case "R":
					viewRankings();
					break;
				case "S":
					selectGame();
					break;
				case "I":
					viewInstructions();
					break;
				case "B":
					createNewGame();
					break;
				case "C":
					input.frame.top();
					hotkeys=false;
					break;
				case "\x1b":
				case "Q":
					return false;
				case KEY_UP:
				case KEY_DOWN:
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_HOME:
				case KEY_END:
					break;
				}
			}
			else {
				if(cmd.length > 0) {
					chat.submit(channel,cmd);
				}
				else {
					menu.frame.top();
					hotkeys=true;
				}
			}
				
			if(hotkeys) {
				gameList();
			}
		}
	}
	function close() {
		client.unsubscribe(game_id,"games");
	}
	function cycle() {
		client.cycle();
		chat.cycle();
		while(client.updates.length) 
			processUpdate(client.updates.shift());
		var chan = chat.channels[channel.toUpperCase()];
		while(chan.messages.length > 0) {
			submitMessage(chatFrame,chan.messages.shift());
		}
		frame.cycle();
	}
	function compressScores(scores) {
		compressed=[];
		for(var s in scores)
			compressed.push(scores[s]);
		return compressed;
	}
	function sortScores(scores) { 
		var scores=compressScores(scores);
		var numScores=scores.length;
		for(n=0;n<numScores;n++) {
			for(m = 0; m < (numScores-1); m++) {
				if(scores[m].points < scores[m+1].points) {
					holder = scores[m+1];
					scores[m+1] = scores[m];
					scores[m] = holder;
				}
			}
		}
		return scores;
	}	
	function sortGameData(array) {
		var sorted=new Object();
		sorted.started=[];
		sorted.waiting=[];
		sorted.yourgames=[];
		sorted.yourturn=[];
		sorted.eliminated=[];
		sorted.singleplayer=[];
		
		for(var i in array) {
			var game=array[i];
			var in_game=findPlayer(game,user.alias);
			
			if(game.single_player) {
				if(in_game>=0) 
					sorted.singleplayer.push(i);
			} 
			else {
				if(in_game>=0) {
					sorted.yourgames.push(i);
				}
				if(game.status==status.PLAYING) {
					sorted.started.push(i);
					if(game.turn==in_game) 
						sorted.yourturn.push(i);
					else if(!game.players[in_game].active)
						sorted.eliminated.push(i);
				}
				else {
					sorted.waiting.push(i);
				}
			}
		}
		return sorted;
	}
	function viewRankings() {
		data.loadScores();
		var scores=sortScores(data.scores);
		scoreFrame.clear();
		wrap(scoreFrame,"\1n\1c " + 
			format("%-18s\1h \1n\1c","PLAYER") + 
			format("%-18s\1h \1n\1c","SYSTEM") + 
			format("%6s\1h \1n\1c","POINTS") + 
			format("%4s\1h \1n\1c","KILL") + 
			format("%3s\1h \1n\1c","WIN") + 
			format("%4s","LOSS")); 
		for(var s=0;s<scores.length;s++) {
			var score=scores[s];
			var color="\1y\1h";
			if(score.name==user.alias)
				color="\1r\1h";
			wrap(scoreFrame,color+ " " + 
				format("%-18s\1h "+color,score.name) + 
				format("%-18s\1h "+color,score.system) + 
				format("%6s\1h "+color,score.points) + 
				format("%4s\1h "+color,score.kills) + 
				format("%3s\1h "+color,score.wins) + 
				format("%4s",score.losses)); 
		}
		scoreFrame.end();
		scoreFrame.center("\1r\1h<SPACE to continue>");
		scoreFrame.open();
		scoreFrame.draw();
		while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
		scoreFrame.close();
	}
	function viewInstructions(section) {

	}
	function wrap(f,msg,lst) {
		f.putmsg(msg);
		if(lst) {
			f.putmsg(" \1w\1h: ");
			var col=32;
			var delimiter="\1n\1g,";
			for(aa=0;aa<lst.length;aa++)
			{
				if(aa==lst.length-1) delimiter="";
				var item=lst[aa]+delimiter;
				if((col + console.strlen(item))>78) {
					console.crlf();
					console.right(31);
					col=32;
				}
				f.putmsg("\1h\1g" + item);
				col += console.strlen(item);
			}
		}
		f.crlf();
	}
	function processUpdate(update) {
	
		var p = update.location.split(".");
		var gnum;
		var obj=data;

		while(p.length > 1) {
			var child=p.shift();
			if(obj[child])
				obj=obj[child];

			switch(child.toUpperCase()) {
			case "GAMES":
				gnum = p[0];
				break;
			}
		}
		
		var child = p.shift();
		obj[child] = update.data;
		gameList();
	}
	function viewGameInfo(game) {
		clearBlock(2,2,78,16);
		console.gotoxy(2,2);
		wrap("\1n\1cGAME #\1h" + game.gameNumber + " \1n\1cINFO");
		wrap("\1n\1cHidden names\1h: \1n\1c" + game.hidden_names);
		wrap("\1n\1cPlayers in this game\1h:");
		for(var p in game.players) {
			var name=game.players[p].name;
			if(game.hidden_names) name="Player " + (Number(p)+1);
			wrap("\1c\1h " + name + " \1n\1cvote\1h: " + game.players[p].vote);
		}
		var player=findPlayer(game,user.alias);
		if(player>=0) {
			response=menuPrompt("Remove yourself from this game? \1w\1h[y/N]",false,true);
			if(response=='Y') {
				game.players.splice(player,1);
				data.removePlayer(game,player)
				return;
			}
			response=menuPrompt("Change your vote? \1w\1h[y/N]",false,true);
			if(response=='Y') {
				current_vote=game.players[player].vote;
				game.players[player].vote=(current_vote?false:true);
			}
		} else {
			response=menuPrompt("Join this game? \1w\1h[y/N]",false,true);
			if(response!=='Y') {
				return;
			}
			vote=menuPrompt("Vote to start? \1w\1h[y/N]",false,false);
			addPlayer(game,user.alias,system.name,(vote=="Y"?true:false));
		}
		
		if(game.players.length>=settings.min_players) {
			var vote_to_start=true;
			for(var p in game.players) {
				if(vote_to_start) 
					vote_to_start=game.players[p].vote;
			}
			if(vote_to_start) {
				startGame(game); 
				return;
			}
		}
		data.saveGame(game);
	}
	function gameList()	{
		listFrame.clear();
		var games = client.read(game_id,"games",1);
		var sorted = sortGameData(games);
		wrap(listFrame,"\1gGames in progress          ",sorted.started);
		wrap(listFrame,"\1gGames needing more players ",sorted.waiting);
		wrap(listFrame,"\1gYou are involved in games  ",sorted.yourgames);
		wrap(listFrame,"\1gIt is your turn in games   ",sorted.yourturn);
		wrap(listFrame,"\1gYou are eliminated in games",sorted.eliminated);
		wrap(listFrame,"\1gSingle-player games        ",sorted.singleplayer);
	}
	function selectGame() {
		if(data.count()==0) {
			menuPrompt("\1n\1cNo games to select \1w\1h[press any key]",false,false,true);
			return false;
		}
		while(1) {
			var num=menuPrompt("Game number: ",false,false);
			if(!num) 
				break;
			if(data.games[num]) {
				if(data.games[num].status>=0) {
					playGame(num);
					return true;
				} else {
					viewGameInfo(data.games[num]);
				}
				break;
			} else {
				menuPrompt("\1n\1c No such game! \1w[press any key]",true,false,true);
			}
		}
		return false;
	}
	function startGame(game) {
		game.status=status.PLAYING;
		if(game.players.length==1) 
			game.single_player=true;

		addComputers(game);
		shufflePlayers(game);
		
		var map = generateMap(game);
		dispersePlayers(game,map);
		
		data.saveGame(game);
		data.saveMap(map);
	}
	function createNewGame() {
		response=menuPrompt("Begin a new game? \1w\1h[Y/n]: ",false,true,true);
		if(response!=='Y') 
			return false;
			
		var single_player = false;
		var start_now = false;
		var hidden_names = false;
		var gameNumber = 1;
			
		while(1) {
			response=menuPrompt("Single player game? \1w\1h[y/N]: ",false,true,true);
			if(response=='\x1b' || response=='Q') {
				return false;
			} else if(response=='Y') {
				single_player = true;
				start_now = true;
				break;
			} else if(response=='N') {
				single_player = false;
				break;
			} else {
				menuPrompt("Invalid response \1w\1h[press any key]",true,true,true);
			}
		}
		
		if(!single_player) {
			while(1) {
				response=menuPrompt("Start when there are enough players? \1w\1h[y/N]: ",false,true,true);
				if(response=='\x1b' || response=='Q') {
					return false;
				} else if(response=='Y') {
					start_now = true;
					break;
				} else if(response=='N') {
					start_now = false;
					break;
				} else {
					menuPrompt("Invalid response \1w\1h[press any key]",true,true,true);
				}
			}
			
			while(1) {
				response=menuPrompt("Hide player names? \1w\1h[y/N]: ",false,true,true);
				if(response=='\x1b' || response=='Q') {
					return false;
				} else if(response=='Y') {
					hidden_names=true;
					break;
				} else if(response=='N') {
					hidden_names=false;
					break;
				} else {
					menuPrompt("Invalid response \1w\1h[press any key]",true,false,true);
				}
			}
		}
		
		client.lock(game_id,"games",2);
		var games = client.read(game_id,"games");
		
		while(games && games[gameNumber])
			gameNumber++;
			
		var game = new Game(gameNumber,hidden_names);
		addPlayer(game,user.alias,system.name,start_now);
		data.games[gameNumber] = game;
		
		client.write(game_id,"games." + gameNumber,game);
		client.unlock(game_id,"games");
		
		if(single_player) {
			startGame(game);
			playGame(gameNumber);
			return true;
		}
		return false;
	}
	function getVote() {
	}
	
	open();
	main();
	close();
}

/* game */
function playGame(gameNumber) {
	var game=data.loadGame(gameNumber);
	var map=data.loadMap(gameNumber);
	var gameBackground=loadGraphic(root+settings.background_file);
	var activityFrame=new Frame(48,12,32,12,BG_BLACK|LIGHTGRAY,frame);
	var gameFrame=new Frame(2,2,45,22,undefined,frame);
	var menuFrame=new Frame(10,24,70,1,BG_BROWN|BLACK,frame);
	var listFrame=new Frame(48,2,18,9,undefined,frame);
	var infoFrame=new Frame(67,2,13,9,undefined,frame);
	var battleFrame=new Frame(30,8,20,8,BG_BLUE,frame);
	var cursor=new Frame(1,1,1,1,BG_LIGHTGRAY|BLACK,frame);
	var localPlayer=findPlayer(game,user.alias);
	var channel="dicewarz2_" + gameNumber;
	var menu;
	
	/* stateful shit */
	var update=true;
	var taking_turn=false;
	var coords=false;
	var hotkeys=true;
	
	/* init shit */
	function open() {
		client.subscribe(game_id,"maps." + game.gameNumber);
		client.subscribe(game_id,"metadata." + game.gameNumber);
		var menuItems=[
			"~Take turn",
			"~Attack", 
			"~End turn",
			"~Help",
			"~Forfeit",
			"~Chat",
			"~Quit"
		];
		
		menu=new Menu(menuItems,menuFrame,"\1w\1h","\1n\1k");	

		setMenuCommands();
		chat.join(channel);
		activityFrame.lf_strict=false;
		gameFrame.transparent = true;
		gameBackground.open();
		activityFrame.open();
		menuFrame.open();
		gameFrame.open();
		listFrame.open();
		infoFrame.open();
		
		cursor.putmsg("\xC5");

		statusAlert();
		drawMap(map);
		listPlayers();
	}
	function close() {
		client.unsubscribe(game_id,"maps." + game.gameNumber);
		client.unsubscribe(game_id,"metadata." + game.gameNumber);
		gameBackground.delete();
		activityFrame.delete();
		menuFrame.delete();
		gameFrame.delete();
		listFrame.delete();
		infoFrame.delete();
		chat.part(channel.toUpperCase());
	}
	function main()	{
		
		while(1) {
			cycle();
			var cmd=input.getkey(hotkeys);
			if(!cmd)
				continue;
			if(hotkeys) {
				if(menu.items[cmd.toUpperCase()] && menu.items[cmd.toUpperCase()].enabled) {
					switch(cmd.toUpperCase()) {
					case "I":
						viewInstructions();
						break;
					case "T":
						takeTurn();
						break;
					case "A":
						promptFrame.open();
						cursor.open();
						coords=attack(coords);
						cursor.close();
						promptFrame.close();
						listPlayers();
						break;
					case "E":
						endTurn();
						break;
					case "F":
						forfeit();
						break;
					case "C":
						input.frame.top();
						hotkeys=false;
						break;
					case "R":
						redraw();
						break;
					case "\x1b":
					case "Q":
						return;
					}
					setMenuCommands();
				} 
			} 
			else {
				if(cmd.length > 0) {
					chat.submit(channel,cmd);
				}
				else {
					menu.frame.top();
					hotkeys=true;
				}
			}
		}
	}
	function cycle() {
		client.cycle();
		chat.cycle();
		while(client.updates.length) 
			processUpdate(client.updates.shift());
		var chan = chat.channels[channel.toUpperCase()];
		while(chan.messages.length > 0) {
			submitMessage(activityFrame,chan.messages.shift());
		}
		frame.cycle();
	}
	function processUpdate(update) {
	
		var p = update.location.split(".");
		var gnum;
		var mnum;
		var pnum;
		var tnum;
		
		var obj=data;

		while(p.length > 1) {
			var child=p.shift();
			if(obj[child])
				obj=obj[child];

			switch(child.toUpperCase()) {
			case "GAMES":
				gnum = p[0];
				break;
			case "MAPS":
				mnum = p[0];
				break;
			case "PLAYERS":
				pnum = p[0];
				break;
			case "TILES":
				tnum = p[0];
				break;
			}
		}
		
		var child = p.shift();
		obj[child] = update.data;

		switch(child.toUpperCase()) { 
		case "STATUS":
		case "WINNER":
		case "ROUND":
			listPlayers();
			break;
		case "TURN":
			log("updating turn");
			setMenuCommands();
			statusAlert();
			break;
		case "ACTIVITY":
			msgAlert(update.data);
			return;
		}
		
		if(tnum) {
			drawTile(map,map.tiles[tnum]);
		}
	}
	function setMenuCommands()	{
		update=true;
		if(localPlayer>=0 && game.status==status.PLAYING) {
			if(taking_turn) {
				menu.enable("A");
				menu.enable("E");
				menu.enable("F");
				menu.disable("T");
				return;
			} 
			if(localPlayer==game.turn) {
				menu.enable("T");
				menu.disable("A");
				menu.disable("E");
				menu.disable("F");
				return;
			} 
		}
		menu.disable("T");
		menu.disable("A");
		menu.disable("E");
		menu.disable("F");
	}

	/* gameplay shit */
	function forfeit() {
		taking_turn=false;
		game.players[localPlayer].active=false;
		data.savePlayer(game,localPlayer);
		data.scoreForfeit(game.players[localPlayer]);
		updateStatus(game,map);
		statusAlert();
		listPlayers();
	}
	function endTurn() {
		taking_turn=false;

		reinforce();
		nextTurn(game);
		
		updateStatus(game,map);
		
		listPlayers();
		statusAlert();
	}
	function reinforce() {
		var update=getReinforcements(game,map);
		var player=game.players[localPlayer];
		for(var t in update.tiles) {
			var tile = map.tiles[t];
			drawSector(map,tile.home.x,tile.home.y);
		}
		if(update.placed>0) {
			var msg="\1n\1y" + player.name + " placed " + update.placed + " dice";
			data.saveActivity(game,msg);
			msgAlert(msg);
		}
		if(update.reserved>0) {
			var msg="\1n\1y" + player.name + " reserved " + update.reserved + " dice";
			data.saveActivity(game,msg);
			msgAlert(msg);
		}
	}
	function takeTurn() {
		taking_turn=true;
	}
	function attack(coords) {
		if(!coords) 
			coords=new Coords(map.width/2,map.height/2,0);
		var attacking;
		var attacker=game.players[localPlayer].name;
		var defending;
		var defender;
		
		menuText("Choose a territory to attack from. Press 'Q' to stop attacking.");
		while(1) {
			cycle();
			coords=select(coords);
			if(!coords) {
				return false;
			}
			attacking=map.tiles[map.grid[coords.x][coords.y]];
			if(attacking.owner==localPlayer) {
				if(attacking.dice>1) 
					break;
				else 
					msgAlert("\1r\1hNot enough dice to attack");
			} else {
				msgAlert("\1r\1hNot your territory");
			}
		}
		menuText("Choose a territory to attack. Press 'Q' to stop attacking.");
		while(1) {
			cycle();
			coords=select(coords);
			if(!coords) return false;
			defending=map.tiles[map.grid[coords.x][coords.y]];
			if(defending.owner!=localPlayer) {
				if(connected(attacking,defending,map)) {
					defender=game.players[defending.owner].name;
					break;
				} else {
					msgAlert("\1r\1hNot connected");
				}
			} else {
				msgAlert("\1r\1hNot an enemy territory");
			}
		}

		var a=new Roll(attacking.owner);
		for(var r=0;r<attacking.dice;r++) {
			var roll=random(6)+1;
			a.roll(roll);
		}
		//showRoll(a.rolls,LIGHTGRAY+BG_RED,chatView.x,chatView.y);
		var d=new Roll(defending.owner);
		for(var r=0;r<defending.dice;r++) {
			var roll=random(6)+1;
			d.roll(roll);
		}
		//showRoll(d.rolls,BLACK+BG_LIGHTGRAY,chatView.x,chatView.y+3);
		battle(a,d);
		
		if(a.total>d.total) {
			if(countTiles(map,defending.owner)==1) {
				data.scoreKiller(game.players[attacking.owner]);
				data.scoreLoser(game.players[defending.owner]);
				var msg = "\1r\1h" + attacker + " eliminated " + defender + "!";
				data.saveActivity(game,msg);
				msgAlert(msg);
			}
			data.assignTile(map,defending,attacking.owner,attacking.dice-1);
			drawTile(map,defending);
			updateStatus(game,map);
		} 
		data.assignTile(map,attacking,attacking.owner,1);
		drawSector(map,attacking.home.x,attacking.home.y);
		return coords;
	}
	function battle(a,d) {
		msgAlert("\1n" + game.players[a.pnum].name + " vs. " + game.players[d.pnum].name);
		msgAlert("\1n" + game.players[a.pnum].name + " rolls " + 
			a.rolls.length + (a.rolls.length>1?" dice":" die")+"\1h: \1c" + a.total);
		msgAlert("\1n" + game.players[d.pnum].name + " rolls " + 
			d.rolls.length + (d.rolls.length>1?" dice":" die")+"\1h: \1c" + d.total);
	}
	function select(start) {
		var posx=start.x;
		var posy=start.y;
		var yoffset=start.z;
		
		var coords = getxy(posx,posy);
		cursor.moveTo(coords.x+2,coords.y+2);
		if(yoffset>0) 
			cursor.move(0,1);
		
		while(1) {
			var xoffset=posx%2;
			var cmd=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
			if(cmd) {
				drawSector(map,posx,posy);
				switch(cmd) {
				case "R":
					redraw();
					break;
				case "C":
					chatInput();
					break;
				case KEY_UP:
					if(posy<=0) 
						break;
					if(yoffset==1) 
						yoffset--;
					else {
						yoffset++;
						posy--;
					}
					cursor.move(0,-1);
					break;
				case KEY_DOWN:
					if(posy==map.height-1 && yoffset==1) 
						break;
					if(posy>=map.height) 
						break;
					if(yoffset==0) 
						yoffset++;
					else {
						yoffset--;
						posy++;
					}
					cursor.move(0,1);
					break;
				case KEY_LEFT:
					if(posx==0) 
						break;
					if(yoffset==0) {
						yoffset++;
						if(xoffset==0) posy--;
					} else {
						yoffset--;
						if(xoffset==1) posy++;
					}
					posx--;
					cursor.move(-2,0);
					break;
				case KEY_RIGHT:
					if(posx==map.width-1) 
						break;
					if(yoffset==0) {
						yoffset++;
						if(xoffset==0) 
							posy--;
					} else {
						yoffset--;
						if(xoffset==1) 
							posy++;
					}
					posx++;
					cursor.move(2,0);
					break;
				case KEY_HOME:
					if(posx==0) 
						posx=map.width-1;
					else 
						posx=0;
					posy=0;
					cursor.moveTo(gameFrame.x,gameFrame.y);
					break;
				case KEY_END:
					if(posx==0) 
						posx=map.width-1;
					else 
						posx=0;
					posy=map.height-1;
					cursor.moveTo(gameFrame.x+gameFrame.width,gameFrame.y+gameFrame.height);
					break;
				case "\r":
				case "\n":
					var tile=map.grid[posx][posy];
					var north=map.grid[posx][posy-1];
					var valid=true;
					if(yoffset==1) {
						if(tile==undefined) valid=false;
					} else if(tile==undefined && north==undefined) {
						valid=false;
					}
					else if(tile!==north) {
						if(north>=0 && tile==undefined) 
							posy-=1;
						else if(tile>=0 && north>=0) 
							valid=false;
					}
					if(valid) {
						var position=new Coords(posx,posy,yoffset);
						return position;
					}
					msgAlert("\1r\1hInvalid selection");
					break;
				case "\x1b":
				case "Q":
					return false;
				default:
					break;
				}
			}
			cycle();
		}
		
	}

	/* display shit */
	function getPlayerColors(p) {
		var colors=new Object();
		colors.bg=getColor(settings.background_colors[p]);
		colors.fg=getColor(settings.foreground_colors[p]);
		colors.txt=getColor(settings.text_colors[p]);
		return colors;
	}
	function statusAlert() {
		//TODO: use frame for this?
		if(game.status==status.FINISHED) {
			msgAlert("\1y\1hGame ended.");
			msgAlert("\1y\1h" + game.players[game.winner].name + " has won.");
		} else {
			if(game.turn==localPlayer) {
				msgAlert("\1c\1hIt is your turn");
				menu.draw();
			}
			else {
				msgAlert("\1n\1cIt is " + game.players[game.turn].name + "'s turn");
			}
		}
	}
	function msgAlert(msg) {
		activityFrame.putmsg(msg + "\r\n");
	}
	function getxy(x,y) {
		var offset=(x%2);
		var posx=(x*2)+1;
		var posy=(y*2)+offset;
		var coords=new Coords(posx,posy);
		//console.gotoxy(coords);
		return coords;
	}
	function drawTile(map,tile) {
		for(var c=0;c<tile.coords.length;c++) {
			var coords=tile.coords[c];
			drawSector(map,coords.x,coords.y);
		}
		var borders=getBorders(map,tile);
		for(var x in borders) {
			if(borders[x]) {
				for(var y in borders[x]) 
					drawSector(map,x,y);
			}
		}
	}
	function drawSector(map,x,y) {
		var current_coords=new Coords(x,y);
		var neighbors=getNeighboringSectors(current_coords,map.grid);
		var current=getTile(map.grid,current_coords);
		var north=getTile(map.grid,neighbors.north);
		var northwest=getTile(map.grid,neighbors.northwest);
		var northeast=getTile(map.grid,neighbors.northeast);
		var south=getTile(map.grid,neighbors.south);
		var southwest=getTile(map.grid,neighbors.southwest);
		var southeast=getTile(map.grid,neighbors.southeast);
		
		var top_char;
		var left_char;
		var middle_char;
		var right_char;
		var bottom_char;

		var current_colors;
		var current_tile;
		
		if(current >= 0)	{
			var current_tile=map.tiles[current];
			current_colors=getPlayerColors(current_tile.owner);
			//MIDDLE CHAR
			if(x==current_tile.home.x && y==current_tile.home.y)
				middle_char=new Char(current_tile.dice,current_colors.txt,current_colors.bg);
			else
				middle_char=new Char(" ",current_colors.fg,current_colors.bg);
			//TOP CHAR
			if(north >= 0) {
				var north_tile=map.tiles[north];
				var north_colors=getPlayerColors(north_tile.owner);
				if(north==current)
					top_char=new Char(" ",current_colors.fg,current_colors.bg);
				else if(north_tile.owner==current_tile.owner)
					top_char=new Char("\xC4",BLACK,current_colors.bg);
				else 
					top_char=new Char("\xDF",north_colors.fg,current_colors.bg);
			} else {
				top_char=new Char("\xDC",current_colors.fg,BG_BLACK);
			}
			//LEFT CHAR
			if(northwest==current && southwest==current) {
				left_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else if(northwest==southwest) {
				if(northwest>=0 && map.tiles[northwest].owner==current_tile.owner) {
					left_char=new Char("\xB3",BLACK,current_colors.bg);
				} else if(northwest>=0) {
					var colors=getPlayerColors(map.tiles[northwest].owner);
					left_char=new Char("\xDD",colors.fg,current_colors.bg);
				} else {
					left_char=new Char("\xDE",current_colors.fg,BG_BLACK);
				}
			} else if(northwest>=0 && southwest>=0 && map.tiles[northwest].owner==current_tile.owner && map.tiles[southwest].owner==current_tile.owner && northwest!=current && southwest!=current) {
				left_char=new Char("\xB4",BLACK,current_colors.bg);
			} else if(northwest>=0 && map.tiles[northwest].owner==current_tile.owner && northwest!=current) {
				left_char=new Char("\xD9",BLACK,current_colors.bg);
			} else if(southwest>=0 && map.tiles[southwest].owner==current_tile.owner && southwest!=current) {
				left_char=new Char("\xBF",BLACK,current_colors.bg);
			} else if(northwest>=0 || southwest>=0) {
				left_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else {
				left_char=new Char("\xDE",current_colors.fg,BG_BLACK);
			}
			//RIGHT CHAR
			if(northeast==current && southeast==current) {
				right_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else if(northeast==southeast) {
				if(northeast>=0 && map.tiles[northeast].owner==current_tile.owner) {
					right_char=new Char("\xB3",BLACK,current_colors.bg);
				} else	if(northeast>=0) {
					var colors=getPlayerColors(map.tiles[northeast].owner);
					right_char=new Char("\xDE",colors.fg,current_colors.bg);
				} else {
					right_char=new Char("\xDD",current_colors.fg,BG_BLACK);
				}
			} else if(northeast>=0 && southeast>=0 && map.tiles[northeast].owner==current_tile.owner && map.tiles[southeast].owner==current_tile.owner && northeast!=current && southeast!=current) {
				right_char=new Char("\xC3",BLACK,current_colors.bg);
			} else if(northeast>=0 && map.tiles[northeast].owner==current_tile.owner && northeast!=current) {
				right_char=new Char("\xC0",BLACK,current_colors.bg);
			} else if(southeast>=0 && map.tiles[southeast].owner==current_tile.owner && southeast!=current) {
				right_char=new Char("\xDA",BLACK,current_colors.bg);
			} else if(northeast>=0 || southeast>=0) {
				right_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else {
				right_char=new Char("\xDD",current_colors.fg,BG_BLACK);
			}
			//BOTTOM CHAR
			if(south<0) {
				bottom_char=new Char("\xDF",current_colors.fg,BG_BLACK);
			}
		} else {
			middle_char=new Char("~",BLUE,BG_BLACK);
			if(north >= 0) {
				var north_tile=map.tiles[north];
				var north_colors=getPlayerColors(north_tile.owner);
				top_char=new Char("\xDF",north_colors.fg,BG_BLACK);
			} else	top_char=new Char(" ",BLACK,BG_BLACK);
			if(northwest >= 0 && northwest==southwest) {
				var southwest_colors=getPlayerColors(map.tiles[southwest].owner);
				left_char=new Char("\xDD",southwest_colors.fg,BG_BLACK);
			} else left_char=new Char(" ",BLACK,BG_BLACK);
			if(northeast >= 0 && northeast==southeast) {
				var southeast_colors=getPlayerColors(map.tiles[southeast].owner);
				right_char=new Char("\xDE",southeast_colors.fg,BG_BLACK);
			} else right_char=new Char(" ",BLACK,BG_BLACK);
		}

		//TODO: use frame for this? 
		var coords = getxy(x,y);
		gameFrame.setData(coords.x,coords.y,top_char.ch,top_char.bg|top_char.fg);
		//top_char.draw();
		//console.down();
		//console.left(2);
		//left_char.draw();
		gameFrame.setData(coords.x-1,coords.y+1,left_char.ch,left_char.bg|left_char.fg);
		//middle_char.draw();
		gameFrame.setData(coords.x,coords.y+1,middle_char.ch,middle_char.bg|middle_char.fg);
		//right_char.draw();
		gameFrame.setData(coords.x+1,coords.y+1,right_char.ch,right_char.bg|right_char.fg);
		if(bottom_char) {
			//console.down();
			//console.left(2);
			//bottom_char.draw();
			gameFrame.setData(coords.x,coords.y+2,bottom_char.ch,bottom_char.bg|bottom_char.fg);
		}
	}
	function drawMap(map) {
		for(var x=0;x<map.grid.length;x++) {
			for(var y=0;y<map.grid[x].length;y++) {
				drawSector(map,x,y);
			}
		}
	}
	function drawTiles(map) {
		for(var t in map.tiles)
			drawTile(map,map.tiles[t]);
	}
	function listPlayers() {
		for(var p=0;p<game.players.length;p++) {
			var plyr=game.players[p];
			var fg=plyr.active?getColor(settings.foreground_colors[p]):BLACK;
			var bg=plyr.active?getColor(settings.background_colors[p]):BG_BLACK;
			var txt=plyr.active?getColor(settings.text_colors[p]):DARKGRAY;
			
			listFrame.gotoxy(1,p+1);
			listFrame.putmsg("\xDE",BG_BLACK|fg);
			listFrame.putmsg(printPadded(plyr.name,16),bg|txt);
			listFrame.putmsg("\xDD",BG_BLACK|fg);
			
			infoFrame.gotoxy(1,p+1);
			infoFrame.putmsg(printPadded(countTiles(map,p),4," ","right"),bg|txt);
			infoFrame.putmsg("\xB3",bg|BLACK);
			infoFrame.putmsg(printPadded(countDice(map,p),3," ","right"),bg|txt);
			infoFrame.putmsg("\xB3",bg|BLACK);
			infoFrame.putmsg(printPadded(plyr.reserve+" ",4," ","right"),bg|txt);
		}
	}
	function redraw() {
		//frame.invalidate();
	}
	
	open();
	main();
	close();
}

open();
lobby();
close();
