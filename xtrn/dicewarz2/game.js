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
var useralias = user.alias.replace(/\./g,"_");

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
		var str = getColor(chat.colors.NICK_COLOR) + msg.nick.name + "\1n: " + 
		getColor(chat.colors.TEXT_COLOR) + msg.str;
	else
		var str = getColor(chat.colors.NOTICE_COLOR) + msg.str;
	f.putmsg(str + "\r\n");
}

/* main lobby */
function lobby() {
	var layout=new Layout(frame);
	var lobbyBackground=loadGraphic(root+settings.lobby_file);
	var listFrame=new Frame(2,2,78,13,undefined,frame);
	var menuFrame=new Frame(10,24,70,1,BG_BROWN|BLACK,frame);
	var chatFrame=new Frame(2,16,78,8,BG_BLACK|LIGHTGRAY,frame);
	var scoreFrame=new Frame(5,5,70,15,BG_BLUE|YELLOW,frame);
	var infoFrame=new Frame(20,8,40,10,BG_BLUE,frame);
	var helpFrame = new Frame(1,1,80,24,BG_BLACK|LIGHTGRAY,frame);
	var helpInfoBar = new Frame(1,24,80,1,BG_BLUE|YELLOW,helpFrame);
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
		loadInstructions();
		gameList();
	}	
	function main() {
		var hotkeys=true;
		while(1) {
			cycle();
			var cmd=input.getkey(hotkeys);
			if(cmd === undefined) 
				continue;
			switch(cmd) {
			case KEY_UP:
				chatFrame.scroll(0,-1);
				continue;
			case KEY_DOWN:
				chatFrame.scroll(0,1);
				continue;
			case KEY_HOME:
				chatFrame.pageup();
				continue;
			case KEY_END:
				chatFrame.pagedown();
				continue;
			}
			if(hotkeys) {
				//menu.clear();
				switch(cmd.toUpperCase())	{
				case "R":
					scoreFrame.open();
					viewRankings();
					scoreFrame.close();
					break;
				case "S":
					selectGame();
					break;
				case "H":
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
		chat.disconnect();
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
		var sorted= {
			started:[],
			waiting:[],
			yourgames:[],
			yourturn:[],
			eliminated:[],
			singleplayer:[]
		};
		
		for(var i in array) {
			var game=array[i];
			var in_game=findPlayer(game,useralias);
			
			if(game.single_player) {
				sorted.singleplayer.push(i);
				if(in_game >= 0) {
					sorted.yourgames.push(i);
				}
				if(game.turn == in_game) {
					sorted.yourturn.push(i);
				}
			} 
			else {
				if(in_game>=0) {
					sorted.yourgames.push(i);
				}
				if(game.status==status.PLAYING) {
					sorted.started.push(i);
					if(game.turn==in_game) {
						sorted.yourturn.push(i);
					}
					else if(in_game>=0 && !game.players[in_game].active) {
						sorted.eliminated.push(i);
					}
				}
				else {
					sorted.waiting.push(i);
				}
			}
		}
		for each(var s in sorted ) {
			s.sort(function (a,b) { return a-b });
		}
		return sorted;
	}
	function viewRankings() {
		scoreFrame.clear();
		data.loadScores();
		var count = 0;
		var scores_per_page = 13;
		var scores=sortScores(data.scores);
		for(var s=0;s<scores.length;s++) {
			if(count > 0 && count%scores_per_page == 0) {
				scoreFrame.end();
				scoreFrame.center("\1r\1h<SPACE to continue>");
				scoreFrame.draw();
				while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
				scoreFrame.clear();
			}
			if(count++%scores_per_page == 0) {
				wrap(scoreFrame,"\1n\1c " + 
					format("%-20s\1h \1n\1c","PLAYER") + 
					format("%-26s\1h \1n\1c","SYSTEM") + 
					format("%6s\1h \1n\1c","POINTS") + 
					format("%4s\1h \1n\1c","KILL") + 
					format("%3s\1h \1n\1c","WIN") + 
					format("%4s","LOSS")); 
			}
			var score=scores[s];
			var color="\1y\1h";
			if(score.name==useralias)
				color="\1g\1h";
			wrap(scoreFrame,color+ " " + 
				format("%-20s\1h "+color,score.name.substr(0,20)) + 
				format("%-26s\1h "+color,score.system.substr(0,26)) + 
				format("%6s\1h "+color,score.points) + 
				format("%4s\1h "+color,score.kills) + 
				format("%3s\1h "+color,score.wins) + 
				format("%4s",score.losses)); 
		}
		scoreFrame.end();
		scoreFrame.center("\1r\1h<SPACE to continue>");
		scoreFrame.draw();
		while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
	}
	function loadInstructions() {
		helpInfoBar.putmsg("Commands: UP, DOWN, HOME, END, Q");
		var f=new File(root+settings.help_file);
		if (!f.open("r", true)) {
			log(LOG_INFO, "error opening help file: " + f.name);
			return false;
		}
		var lines=f.readAll();
		f.close();
		while(lines.length > 0) {
			helpFrame.putmsg(lines.shift() + "\r\n");
		}
	}
	function viewInstructions() {

		helpFrame.scrollTo(0,0);
		helpFrame.open();
		
		while(!js.terminated) {
			helpFrame.cycle();
			var cmd = console.getkey(K_NOSPIN|K_NOECHO|K_UPPER);
			switch(cmd) {
				case KEY_UP:
					helpFrame.scroll(0,-1);
					break;
				case KEY_DOWN:
					helpFrame.scroll(0,1);
					break;
				case KEY_HOME:
					helpFrame.pageup();
					break;
				case KEY_END:
					helpFrame.pagedown();
					break;
				case "Q":
					helpFrame.close();
					return;
			}
		}
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
		if(update.oper == "WRITE") {
			/* todo: make this smarter */
			gameList();
		}
	}
	function viewGameInfo(game) {
		infoFrame.clear();
		infoFrame.crlf();
		wrap(infoFrame,"\1n\1c Game \1h#\1y" + game.gameNumber);
		wrap(infoFrame,"\1n\1c Players\1h:");
		for(var p in game.players) {
			var name=game.players[p].name;
			if(game.hidden_names) 
				name="[Hidden]";
			var vote = game.players[p].vote?"\1g\1hstart":"\1r\1hwait";
			wrap(infoFrame,"\1g\1h  " + name + " \1n\1cvotes to " + vote);
		}
		infoFrame.draw();
		
		var player=findPlayer(game,useralias);
		if(player>=0) {
			if(askRemove(game,player))
				return;
			askChange(game,player);
		} else {
			askJoin(game);
		}
		
		if(game.players.length>=settings.min_players) {
			var vote_to_start=true;
			for(var p in game.players) {
				if(vote_to_start) 
					vote_to_start=game.players[p].vote;
				else
					break;
			}
			if(vote_to_start) {
				startGame(game); 
				data.games[game.gameNumber]=game;
				client.write(game_id,"games." + game.gameNumber,game);
				return;
			}
		}
	}
	function askRemove(game,player) {
		var response=menuPrompt("Remove yourself from this game? [y/N]",false,false,true);
		if(response=='Y') {
			game.players.splice(player,1);
			return true;
		}
		return false
	}
	function askChange(game,player) {
		var response=menuPrompt("Change your vote? [y/N]",false,false,true);
		if(response=='Y') {
			var vote=game.players[player].vote;
			game.players[player].vote=(!vote);
		}
	}
	function askJoin(game) {
		var response=menuPrompt("Join this game? [y/N]",false,false,true);
		if(response!=='Y') {
			infoFrame.close();
			return;
		}
		var vote=menuPrompt("Vote to start? [y/N]",false,false,true);
		addPlayer(game,useralias,system.name,(vote=="Y"?true:false));
	}
	function canView(gameNumber) {
		//todo
		return true;
	}
	function gameList()	{
		listFrame.clear();
		data.loadGames();
		var sorted = sortGameData(data.games);
		wrap(listFrame,"\1gGames in progress          ",sorted.started);
		wrap(listFrame,"\1gGames needing more players ",sorted.waiting);
		wrap(listFrame,"\1gYou are involved in games  ",sorted.yourgames);
		wrap(listFrame,"\1gIt is your turn in games   ",sorted.yourturn);
		wrap(listFrame,"\1gYou are eliminated in games",sorted.eliminated);
		wrap(listFrame,"\1gSingle-player games        ",sorted.singleplayer);
	}
	function selectGame() {
		if(data.count()==0) {
			menuPrompt("No games to select [press any key]",false,false,true);
			return false;
		}
		while(1) {
			var num=numberPrompt("Select game # or ENTER cancel: ",false,false);
			if(!num) 
				break;
			if(data.games[num]) {
				if(data.games[num].status>=0) {
					playGame(num);
					return true;
				} else {
					infoFrame.open();
					client.lock(game_id,"games." + num,2);
					viewGameInfo(data.games[num]);
					client.write(game_id,"games." + num,data.games[num]);
					client.unlock(game_id,"games." + num);
					infoFrame.close();
				}
				break;
			} else {
				menuPrompt("No such game! [press any key]",false,false,true);
				return false;
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
		data.saveMap(map);
	}
	function createNewGame() {
		response=menuPrompt("Begin a new game? \1w\1h[Y/n]: ",false,false,true);
		if(response != 'Y' && response != '\r') 
			return false;
			
		var single_player = false;
		var start_now = false;
		var hidden_names = false;
		var gameNumber = 1;
			
		while(1) {
			response=menuPrompt("Single player game? \1w\1h[y/N]: ",false,false,true);
			if(response=='\x1b' || response=='Q') {
				return false;
			} else if(response=='Y') {
				single_player = true;
				start_now = true;
				break;
			} else if(response=='N' || response=='\r') {
				single_player = false;
				break;
			} else {
				menuPrompt("Invalid response \1w\1h[press any key]",false,false,true);
			}
		}
		
		if(!single_player) {
			while(1) {
				response=menuPrompt("Start when there are enough players? \1w\1h[y/N]: ",false,false,true);
				if(response=='\x1b' || response=='Q') {
					return false;
				} else if(response=='Y') {
					start_now = true;
					break;
				} else if(response=='N' || response=='\r') {
					start_now = false;
					break;
				} else {
					menuPrompt("Invalid response \1w\1h[press any key]",false,false,true);
				}
			}
			
			while(1) {
				response=menuPrompt("Hide player names? \1w\1h[y/N]: ",false,false,true);
				if(response=='\x1b' || response=='Q') {
					return false;
				} else if(response=='Y') {
					hidden_names=true;
					break;
				} else if(response=='N' || response=='\r') {
					hidden_names=false;
					break;
				} else {
					menuPrompt("Invalid response \1w\1h[press any key]",false,false,true);
				}
			}
		}
		
		client.lock(game_id,"games",2);
		var games = client.read(game_id,"games");
		
		while(games && games[gameNumber])
			gameNumber++;
			
		var game = new Game(gameNumber,hidden_names);
		addPlayer(game,useralias,system.name,start_now);
		data.games[gameNumber] = game;
		
		if(single_player) {
			startGame(game);
		}
		
		data.games[gameNumber]=game;
		client.write(game_id,"games." + gameNumber,game);
		client.unlock(game_id,"games");
		
		if(single_player) {
			playGame(gameNumber);
			return true;
		}
		return false;
	}
	function menuPrompt(string, append, keepOpen, hotkeys) {
		if(!append) 
			promptFrame.clear();
		if(!promptFrame.is_open) 
			promptFrame.open();
		promptFrame.putmsg(string);
		promptFrame.draw();
		
		var cmd="";
		while(!js.terminated) {
			var k = console.getkey(K_NOECHO|K_NOCRLF|K_UPPER|K_NOSPIN);
			if(hotkeys) { 
				cmd = k;
				break;
			}
			if(k == "\r") {
				break;
			}
			cmd+=k;
			promptFrame.putmsg(k);
			cycle();
		}
		if(!keepOpen)
			promptFrame.close();
		return cmd;
	}
	function numberPrompt(string, append, keepOpen) {
		if(!append) 
			promptFrame.clear();
		if(!promptFrame.is_open) 
			promptFrame.open();
		promptFrame.putmsg(string);
		promptFrame.draw();
		
		var cmd=[];
		while(!js.terminated) {
			var k = console.getkey(K_NOECHO|K_NOCRLF|K_NUMBER|K_NOSPIN);
			if(k == "\r") {
				break;
			}
			else if(k == "\b") {
				if(cmd.length > 0) {
					cmd.pop();
					promptFrame.putmsg(k);
				}
			}
			else {
				cmd.push(k);
				promptFrame.putmsg(k);
			}
			cycle();
		}
		if(!keepOpen)
			promptFrame.close();
		return cmd.join("");
	}	
	open();
	main();
	close();
}

/* game */
function playGame(gameNumber) {
	var game=client.read(game_id,"games." + gameNumber,1);
	var map=client.read(game_id,"maps." + gameNumber,1);
	var gameBackground=loadGraphic(root+settings.background_file);
	var activityFrame=new Frame(48,12,32,12,BG_BLACK|LIGHTGRAY,frame);
	var gameFrame=new Frame(2,2,45,22,undefined,frame);
	var menuFrame=new Frame(10,24,70,1,BG_BROWN|BLACK,frame);
	var listFrame=new Frame(48,3,18,7,undefined,frame);
	var infoFrame=new Frame(67,3,13,7,undefined,frame);
	var battleFrame=new Frame(30,8,20,8,BG_BLUE,frame);
	var cursor=new Frame(1,1,1,1,BG_LIGHTGRAY|BLACK,frame);
	var localPlayer=findPlayer(game,useralias);
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
		client.subscribe(game_id,"games." + game.gameNumber);
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
		activityFrame.word_wrap=true;
		gameFrame.transparent = true;
		gameBackground.open();
		activityFrame.open();
		menuFrame.open();
		gameFrame.open();
		listFrame.open();
		infoFrame.open();
		
		if(game.round == 0) {
			chat.clear(channel);
			game.round=1;
			data.saveRound(game);
		}

		cursor.putmsg("\xC5");
		statusAlert();
		drawMap(map);
		listPlayers();
	}
	function close() {
		client.unsubscribe(game_id,"maps." + game.gameNumber);
		client.unsubscribe(game_id,"games." + game.gameNumber);
		client.unsubscribe(game_id,"metadata." + game.gameNumber);
		gameBackground.delete();
		activityFrame.delete();
		menuFrame.delete();
		gameFrame.delete();
		listFrame.delete();
		infoFrame.delete();
		inputFrame.bottom();
		chat.part(channel.toUpperCase());
	}
	function main()	{
		
		while(1) {
			cycle();
			var cmd=input.getkey(hotkeys);
			if(cmd === undefined)
				continue;
			switch(cmd) {
			case KEY_UP:
				activityFrame.scroll(0,-1);
				continue;
			case KEY_DOWN:
				activityFrame.scroll(0,1);
				continue;
			case KEY_HOME:
				activityFrame.pageup();
				continue;
			case KEY_END:
				activityFrame.pagedown();
				continue;
			}
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
						doAttacks();
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
	
		if(update.oper == "WRITE") {
			var p = update.location.split(".");
			var gnum;
			var pnum;
			var tnum;
			
			while(p.length > 1) {
				var child=p.shift();
				
				switch(child.toUpperCase()) {
				case "GAMES":
				case "METADATA":
				case "MAPS":
					gnum = p[0];
					break;
				case "PLAYERS":
					pnum = p[0];
					break;
				case "TILES":
					tnum = p[0];
					break;
				}
			}
			
			/* ignore updates not for this game */
			if(gnum != game.gameNumber)
				return false;
			
			if(tnum) {
				map.tiles[tnum] = update.data;
				drawTile(map,map.tiles[tnum]);
				listPlayers();
			}
			
			if(pnum) {
				game.players[pnum] = update.data;
				listPlayers();
			}
			
			var child = p.shift();
			switch(child.toUpperCase()) { 
			case "STATUS":
				game.status = update.data;
				break;
			case "WINNER":
				game.winner = update.data;
				break;
			case "ROUND":
				game.round = update.data;
				break;
			case "TURN":
				game.turn = update.data;
				setMenuCommands();
				statusAlert();
				break;
			case "ACTIVITY":
				msgAlert(update.data);
				return;
			}
		}
		else if(update.oper == "SUBSCRIBE") {
		}
		else if(update.oper == "UNSUBSCRIBE") {
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
	function doAttacks() {
		promptFrame.open();
		cursor.open();
		var coords;
		do {
			coords=attack(coords);
			listPlayers();
		}
		while(coords !== false);
		cursor.close();
		promptFrame.close();
	}
	function forfeit() {
		taking_turn=false;
		game.players[localPlayer].active=false;
		data.savePlayer(game,localPlayer);
		data.saveActivity(game.gameNumber,"\1n\1r" + game.players[localPlayer].name + " forfeits.");
		data.scoreForfeit(game.players[localPlayer]);
		nextTurn(game);
		updateStatus(game,map);
		statusAlert();
		listPlayers();
	}
	function endTurn() {
		taking_turn=false;
		reinforce(localPlayer);
		
		nextTurn(game);
		updateStatus(game,map);
		
		listPlayers();
		statusAlert();
	}
	function reinforce(playerNum) {
		var update=getReinforcements(game,map,playerNum);
		var player=game.players[playerNum];
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
			
		var attacker=game.players[localPlayer];
		var defender;

		var attacking;
		var defending;
		
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
			} 
			else {
				msgAlert("\1r\1hNot your territory");
			}
		}
		menuText("Choose a territory to attack. Press 'Q' to stop attacking.");
		while(1) {
			cycle();
			coords=select(coords);
			if(!coords) {
				return false;
			}
			defending=map.tiles[map.grid[coords.x][coords.y]];
			if(defending.owner!=localPlayer) {
				if(connected(attacking,defending,map)) {
					defender=game.players[defending.owner];
					break;
				} 
				else {
					msgAlert("\1r\1hNot connected");
				}
			} 
			else {
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
		
		data.saveActivity(game,"\1n\1m" + attacker.name + " attacked " + defender.name);
		if(a.total>d.total) {
			if(countTiles(map,defending.owner)==1 && defender.active) {
				data.scoreKiller(attacker);
				data.scoreLoser(defender);
				var msg = "\1r\1h" + attacker.name + " eliminated " + defender.name + "!";
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
		msgAlert("\1n" + game.players[a.pnum].name + " vs " + game.players[d.pnum].name);
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
