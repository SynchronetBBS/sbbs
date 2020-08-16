//$Id: lobby.js,v 1.12 2013/05/29 16:29:04 mcmlxxix Exp $
/*
	JAVASCRIPT TETRIS
	For Synchronet v3.15+
	Matt Johnson(2009)
*/
const VERSION="$Revision: 1.12 $".split(' ')[1];

load("json-chat.js");
load("layout.js");
load("inputline.js");

var game_id="synchronetris";
var root=js.exec_dir;

load(root + "tetrisobj.js");
load(root + "game.js");

var settings = client.read(game_id,"settings",1);
var frame = new Frame();
var data = new GameData();
var status = {WAITING:-1,STARTING:0,SYNCING:1,PLAYING:2,FINISHED:3};


/** GAME LOBBY **/
var lobby=(function() {	

	var chat,layout,chatView,input,menu,gameList,
		hotkeys,profile,index,gnum;

	/* initialize game variables and data */
	function open() {
		//console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status|=SS_PAUSEOFF;
		splash("synchronetris.bin");
		chat = new JSONChat(user.number,undefined,serverAddr,serverPort);
		profile=data.profiles[user.alias];
		layout=new Layout(frame);
		chatView = layout.addView("chat",40,3,40,21);
		chatView.show_border = false;
		chatView.show_title = false;
		input=new InputLine();
		chat.chatView = chatView;
		chat.join("#synchronetris");
		input.init(2,24,78,1,frame,BG_RED|WHITE);
		input.attr = BG_RED|WHITE;
		input.show_cursor = true;
		var menuFrame = new Frame(2,24,78,1,BG_LIGHTGRAY+RED,frame);
		var menuItems = [
			"~Play game",
			"~Leave game",
			"~Ready",
			"~Scores",
			"~Chat",
			"~Help",
			"~Quit"
		];
		hotkeys=true;
		menu=new Menu(menuItems,menuFrame,"\1r\1h","\1n");	
		frame.load(root + "lobby.bin",80,24);
		frame.open();
		layout.open();
		menu.disable("L");
		menu.disable("R");
		menu.frame.top();
		index=1;
		initLobby();
		client.subscribe(game_id,"games");
	}

	/* main loop */
	function main()	{
		while(!js.terminated) {
			cycle();
			var k = input.getkey(hotkeys);
			if(k == undefined) 
				continue;
			if(hotkeys) {
				k = k.toUpperCase();
				switch(k) {
				case KEY_HOME:
					index-=4;
					if(index<1)
						index=1;
					listGames();
					break;
				case KEY_END:
					index+=4;
					listGames();
					break;
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_UP:
				case KEY_DOWN:
					layout.getcmd(k);
					break;
				}
				if(!menu.items[k] || !menu.items[k].enabled)
					continue;
				switch(k.toUpperCase()) {
				case "S":
					showScores();
					break;
				case "H":
				case "?":
					showHelp();
					break;
				case "P":
					joinGame();
					break;
				case "R":
					toggleReady();
					break;
				case "L":
					leaveGame();
					break;
				case "Q":
					return;
				case "C":
					input.frame.top();
					hotkeys=false;
					break;
				}
			}
			else {
				if(k.length > 0) {
					layout.getcmd(k);
				}
				else {
					menu.frame.top();
					hotkeys=true;
				}
			}
		}
	}

	/* clean up and exit */
	function close() {
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		splash("exit.bin");
	}

	/* initiate lobby game info tiles */
	function initLobby() {
		gameList = [
			new GameInfo(2,3),
			new GameInfo(21,3),
			new GameInfo(2,13),
			new GameInfo(21,13)
		];
	}

	/* process subscription data updates */
	function processUpdate(update) {
		var playerName = undefined;
		var gameNumber = undefined;
		
		switch(update.oper) {
		case "WRITE":
			var p=update.location.split(".");
			var obj=data;
			while(p.length > 1) {
				var child=p.shift();
				if(!obj[child])
					obj[child] = {};
				obj=obj[child];
				switch(child.toUpperCase()) {
				case "PLAYERS":
					playerName = p[0];
					break;
				case "GAMES":
					gameNumber = p[0];
					break;
				}
			}
			var child=p.shift();
			if(update.data == undefined)
				delete obj[child];
			else
				obj[child] = update.data;
			data.updated=true;
			
			if(child.toUpperCase() == "STATUS")
				updateStatus(update.data,gameNumber);
			break;
		case "SUBSCRIBE":
		case "UNSUBSCRIBE":
			data.online=client.who(game_id,"games");
			data.updated=true;
			break;
		}
	}

	/* join or start a tetris game */
	function joinGame() {
		/* find the first open game number */
		gnum=getOpenGame();
		if(isNaN(gnum)) {
			log(LOG_WARNING,"Error finding game number");
			return false;
		}
		client.lock(game_id,"games." + gnum,2);
		var game = client.read(game_id,"games." + gnum);
		var player = new Player(profile.name);
		
		/* if the game doesnt exist, create it */
		if(!game) {
			data.games[gnum] = new Game(gnum);
			data.games[gnum].players[profile.name] = player;
			client.write(game_id,"games." + gnum,data.games[gnum]);
		}
		/* otherwise, store player info in game */
		else {
			data.games[gnum] = game;
			data.games[gnum].players[profile.name] = player;
			client.write(game_id,"games."+gnum+".players."+profile.name,player);
		}
		client.unlock(game_id,"games." + gnum);
		menu.enable("L");
		menu.enable("R");
		menu.disable("P");
		data.updated=true;
	}

	/* find the next available game number */
	function getOpenGame() {
		for each(var g in data.games) {
			if(g.status == status.PLAYING || g.status == status.SYNCING)
				continue;
			else if(countMembers(g.players) >= settings.max_players)
				continue;
			return g.gameNumber;
		}
		var g=1;
		while(data.games[g])
			g++;
		return g;
	}

	/* list games in lobby window */
	function listGames() {
		for(var i=0;i<4;i++) {
			var tile = gameList[i];
			var game = data.games[index+i];
			
			if(!game) {
				tile.close();
				continue;
			}
		
			var gameStr = "\1n\1cGame \1c\1h" + game.gameNumber;
			var statusStr = "";
			
			switch(game.status) {
			case status.WAITING:
				statusStr = "  \1n\1c[waiting]";
				break;
			case status.STARTING:
				statusStr = " \1n\1y[starting]";
				break;
			case status.SYNCING:
				statusStr = " \1n\1y[syncing]";
				break;
			case status.PLAYING:
				statusStr = "  \1n\1g[playing]";
				break;
			case status.FINISHED:
				statusStr = " \1k\1h[finished]";
				break;
			default:
				statusStr = "    \1n\1r[error]";
				break;
			}
			
			tile.open();
			tile.frame.home();
			tile.frame.putmsg(splitPadded(gameStr,statusStr,tile.frame.width));
			
			tile.frame.gotoxy(1,8);
			var list = [];
			for each(var p in game.players)
				list.push(p);
			for(var l=0;l<3;l++) {
				var p = list[l];
				if(p) {
					if(p.ready == true)
						tile.frame.putmsg("\1g\1h * ");
					else
						tile.frame.putmsg("\1r\1h * ");
					tile.frame.putmsg("\1n\1g" + p.name);
				}
				tile.frame.cleartoeol();
				tile.frame.crlf();
			}
			tile.frame.crlf();
		}
	}

	/* show rankings */
	function showScores()	{
		data.loadPlayers();
		var scoreFrame = new Frame(12,6,57,14,BG_BLUE + YELLOW,frame);
		var count = 0;
		var scores_per_page = 10;
		var list = sortScores(data.profiles,"score");
		scoreFrame.open();
		for each(var player in list) {
			if(player.score == 0)
				continue;
			if(count > 0 && count%scores_per_page == 0) {
				scoreFrame.center("\1r\1h<SPACE to continue>");
				scoreFrame.draw();
				while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
				scoreFrame.clear();
			}
			if(count++%scores_per_page == 0) {
				scoreFrame.crlf();
				scoreFrame.putmsg("\1w\1h" + format(" %3s %-25s %4s %6s %6s %6s",
					"###","NAME","WINS","LEVEL","SCORE","LINES") + "\r\n");
			}
			scoreFrame.putmsg("\1w\1y" + formatScore(count,	player.name,
				player.wins,player.level,player.score,player.lines) + "\r\n");
		}
		scoreFrame.end();
		scoreFrame.center("\1r\1h<SPACE to continue>");
		scoreFrame.cycle();
		while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
		scoreFrame.delete();
	}
	
	/* format score display */
	function formatScore(num,name,wins,level,score,lines) {
		return format(" \1c%3d \1y%-25s %4d %6d %6d %6d",num,name,wins,level,score,lines); 
	}
	
	/* sort scores */
	function sortScores(scores,property) {
		return sortListByProperty(scores,property);
	}

	/* show game help */
	function showHelp()	{
		var hFrame = new Frame(11,3,60,19,undefined,frame);
		hFrame.open();
		hFrame.load(root + "instructions.bin",60,19);
		hFrame.draw();
		while(console.inkey(K_NOECHO|K_NOCRLF,1) !== " ");
		hFrame.delete();
	}

	/* leave a game */
	function leaveGame() {
		delete data.games[gnum].players[profile.name];
		client.remove(game_id,"games."+gnum+".players."+profile.name,2);
		menu.disable("L");
		menu.disable("R");
		menu.enable("P");
		data.updated=true;
	}

	/* toggle player readiness on/off */
	function toggleReady() {
		var player = data.games[gnum].players[profile.name];
		player.ready = !player.ready;
		client.write(game_id,"games."+gnum+".players."+profile.name+".ready",player.ready,2);
		data.updated=true;
	}

	/* process a game status update */
	function updateStatus(statusUpdate,gameNumber) {
		if(!data.games[gameNumber])
			return false;
		if(data.games[gameNumber].players[profile.name]) {
			switch(statusUpdate) {
			case status.SYNCING:
				playGame(profile,data.games[gameNumber]);
				break;
			}
		}
	}
		
	/* update data sources and display objects */
	function cycle() {
		client.cycle();
		chat.cycle();
		layout.cycle();
		while(client.updates.length > 0)
			processUpdate(client.updates.shift());
		if(data.updated) {
			listGames();
			data.updated = false;
		}
		if(frame.cycle())
			input.popxy();
	}

	/* display a splash screen */
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

	open();
	main();
	close();
})();
