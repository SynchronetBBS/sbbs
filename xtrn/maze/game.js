//$Id: game.js,v 1.18 2017/08/10 03:08:50 mcmlxxix Exp $
/*
	SYNCHRONET MAZE RACE 
	A Javascript remake 
	of the Atari game "Maze Craze"

	For Synchronet v3.15+
	Matt Johnson(2008)
*/
const VERSION="$Revision: 1.18 $".replace(/\$/g,'').split(' ')[1];

var oldpass=console.ctrlkey_passthru;
var root=js.exec_dir;
var game_id = "mazerace";

load("frame.js");
load("json-chat.js");
load("inputline.js");
load("layout.js");
load("graphic.js");
load("funclib.js");
load(root + "mazeobj.js");
load(root + "menu.js");

/* game objects */
var settings=client.read(game_id,"settings",1);
var status={WAITING:-1,STARTING:0,RACING:1,FINISHED:2};
var frame=new Frame(1,1,80,24);
var data=new GameData();

/* game splash screens */
function open() {
	console.ctrlkey_passthru="+ACGKLOPQRTVWXYZ";
	bbs.sys_status|=SS_MOFF;
	bbs.sys_status|=SS_PAUSEOFF;
	splash("mazerace.bin");
}
function close() {
	console.ctrlkey_passthru=oldpass;
	bbs.sys_status&=~SS_MOFF;
	bbs.sys_status&=~SS_PAUSEOFF;
	splash("exit.bin");
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
function formatTime(julian_date) {
	var ms = julian_date%1000;
	julian_date/=1000;
	var s = julian_date%60;
	julian_date/=60;
	var m = julian_date%60;
	julian_date/=60;
	return(format("\1y%2d\1cm \1y%02d\1cs \1y%03d\1cms",m,s,ms));
}
function formatScore(num,name, wins, best_time) {
	return format(" \1c%3d \1y%-25s \1c%4d \1y%13s",num,name,wins,formatTime(best_time)); 
}
function lobby() {

	/* toggles */
	var full_redraw=true;
	var hotkeys=true;
	
	/* display objects */
	var inputFrame,menuFrame,playerFrame,input,chat,
		layout,chatWindow,menu,profile,gnum;

	/* main functions */
	function open()	{
		playerFrame=new Frame(59,5,20,18,BG_BLACK,frame);
		inputFrame=new Frame(3,23,54,1,BG_BLUE + WHITE,frame);
		menuFrame=new Frame(3,23,54,1,BG_BLACK + LIGHTGRAY,frame);
		input=new InputLine(inputFrame);
		chat=new JSONChat(user.number,undefined,serverAddr,serverPort);
		layout=new Layout(frame);
		chatWindow=layout.addView("chat",2,4,56,18)
		menu=new Menu([
			"~Join race",
			"~Leave race",
			"~Ready",
			// "S~ettings",
			"~Damage",
			"~Fog",
			"~Scores",
			"~Chat",
			"~Help",
			"~Quit"
			],
			menuFrame,
			"\1c\1h","\1n"
		);	
		frame.open();
		frame.load(root + "lobby.bin",80,24);
		client.subscribe(game_id,"games");
		chatWindow.show_title=false;
		chatWindow.show_border=false;
		chat.chatView = chatWindow;
		chat.join("#mazerace");
		profile=data.profiles[user.alias.replace(/\./g,"_")];
		menu.disable("L");
		menu.disable("R");
		menu.disable("D");
		menu.disable("F");
		layout.open();
		menu.draw();
		menu.frame.top();
		data.who();
	}
	function main()	{
		while(!js.terminated) {
			cycle();
			var k = input.getkey(hotkeys);
			if(k == undefined) 
				continue;
			if(hotkeys) {
				k = k.toUpperCase();
				switch(k) {
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_UP:
				case KEY_DOWN:
				case KEY_HOME:
				case KEY_END:
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
				case "J":
					joinMaze();
					menu.draw();
					break;
				case "R":
					toggleReady();
					break;
				case "D":
					toggleDamage();
					break;
				case "F":
					toggleFog();
					break;
				case "L":
					leaveMaze();
					menu.draw();
					break;
				case "E":
					todo();
					//chooseAvatar();
					//chooseColor();
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
	function close() {
		frame.close();
		client.unsubscribe(game_id,"games");
	}
	function cycle() {
		client.cycle();
		layout.cycle();
		chat.cycle();
		while(client.updates.length > 0)
			processUpdate(client.updates.shift());
		if(frame.cycle()) {
			if(hotkeys)
				console.gotoxy(1,24);
			else
				input.popxy();
		}
		if(full_redraw)
			redraw();
		if(data.updated) {
			listPlayers();
			data.updated = false;
		}
		if(readyToRace()) {
			data.mazes[gnum] = client.read(game_id,"mazes." + gnum,1);
			race(gnum, profile);
			leaveMaze();
			menu.draw();
		}
	}
	function getGameNumber() {
		for each(var game in data.games) {
			if(game.players && game.players[profile.name] != undefined)
				return game.gameNumber;
		}
		return undefined;
	}
	function readyToRace() {
		if(!data.games[gnum])
			return false;
		if(!data.games[gnum].players[profile.name])
			return false;
		if(data.games[gnum].status != status.RACING)
			return false;
		return (data.games[gnum].players[profile.name].ready);
	}
	function processUpdate(update) {
		try {
			switch(update.oper) {
			case "WRITE":
				var p=update.location.split(".");
				var obj=data;
				while(p.length > 1) {
					var child=p.shift();
					if(!obj[child])
						obj[child] = {};
					obj=obj[child];
				}
				if(update.data == undefined)
					delete obj[p.shift()];
				else
					obj[p.shift()] = update.data;
				data.updated=true;
				break;
			case "SUBSCRIBE":
			case "UNSUBSCRIBE":
				data.online=client.who(game_id,"games");
				data.updated=true;
				break;
			}
		} catch(e) {
			log(LOG_ERROR,e);
			log(LOG_WARNING,update.toSource());
		}
	}
	function chooseAvatar() {
		// var aFrame = new Frame(25,10,30,4,BG_BLUE,frame);
		// aFrame.open();
		// aFrame.crlf();
		// aFrame.putmsg(" " + profile.color + settings.avatars);
		// aFrame.draw();
		// aFrame.center("\1r\1h<SPACE to continue>");
		// aFrame.draw();
		// while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
		// aFrame.delete();
	}
	function chooseColor() {
		//var cf = new Frame(30,10,10,2,BG_BLACK);
	}
	function todo() {
		var tFrame = new Frame(25,10,30,4,BG_BLUE,frame);
		tFrame.open();
		tFrame.crlf();
		tFrame.center("\1y\1hFeature not yet available");
		tFrame.crlf();
		tFrame.crlf();
		tFrame.center("\1r\1h<SPACE to continue>");
		tFrame.draw();
		while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
		tFrame.delete();
	}

	/* helpers */
	function showHelp()	{
		var hFrame = new Frame(11,3,60,19,undefined,frame);
		hFrame.open();
		hFrame.load(root + "instructions.bin",60,19);
		hFrame.draw();
		while(console.inkey(K_NOECHO|K_NOCRLF,1) !== " ");
		hFrame.delete();
	}
	function listPlayers() {
		playerFrame.clear();
		playerFrame.crlf();

		var idle = {};
		for each(var p in data.online) {
			if(!p.nick)
				continue;
			idle[p.nick] = p;
		}
		for each(var g in data.games) {
			playerFrame.putmsg("\1c\1h " + g.gameNumber + ": ");
			if(g.damage)
				playerFrame.putmsg("\1n\1r[D]");
			if(g.fog)
				playerFrame.putmsg("\1k\1h[F]");
			if(g.status == status.WAITING)
				playerFrame.putmsg("\1n\1y[waiting]\r\n");
			else if(g.status == status.STARTING) 
				playerFrame.putmsg("\1y\1h[starting]\r\n");
			else if(g.status == status.RACING)
				playerFrame.putmsg("\1g\1h[racing]\r\n");
			else if(g.status == status.FINISHED)
				playerFrame.putmsg("\1k\1h[finished]\r\n");
			else 
				playerFrame.putmsg("\1n\1r[error]\r\n");
				
			for each(var p in g.players) {
				if(p.ready == true)
					playerFrame.putmsg("\1g\1h * ");
				else
					playerFrame.putmsg("\1r\1h * ");
				playerFrame.putmsg("\1n\1c" + p.name + "\r\n");
				delete idle[p.name];
			}
			playerFrame.crlf();
		}
		if(countMembers(idle) > 0) {
			playerFrame.putmsg("\1c\1h ONLINE:\r\n");
			for each(var p in idle)
				playerFrame.putmsg("\1n\1c  " + p.nick + "\r\n");
		}
	}
	function showScores()	{
		var scoreFrame = new Frame(16,6,50,14,BG_BLUE + YELLOW,frame);
		var count = 0;
		var scores_per_page = 10;
		var list = sortScoresByWins();
		scoreFrame.open();
		for each(var player in list) {
			if(player.wins == 0)
				continue;
			if(count > 0 && count%scores_per_page == 0) {
				scoreFrame.gotoxy(1,24);
				scoreFrame.center("\1r\1h<SPACE to continue>");
				scoreFrame.draw();
				while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
				scoreFrame.clear();
			}
			if(count++%scores_per_page == 0) {
				scoreFrame.crlf();
				scoreFrame.putmsg("\1w\1h" + 
					format(" %3s %-25s %4s %13s","###","NAME","WINS","BEST TIME") + "\r\n");
			}
			scoreFrame.putmsg("\1w\1y" + 
				formatScore(count,player.name,player.wins,player.best_time) + "\r\n");
		}
		scoreFrame.gotoxy(1,24);
		scoreFrame.center("\1r\1h<SPACE to continue>");
		scoreFrame.cycle();
		while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
		scoreFrame.delete();
	}
	function sortScoresByWins() {
		return sortListByProperty(data.profiles,"wins");
	}
	function sortScoresByTime() {
		return sortListByProperty(data.profiles,"best_time").reverse();
	}
	function joinMaze() {
		/* find the first open game number */
		gnum=getOpenGame();
		if(isNaN(gnum)) {
			log(LOG_WARNING,"Error finding game number");
			return false;
		}
		client.lock("mazerace","games." + gnum,2);
		var game = client.read("mazerace","games." + gnum);
		var player = new Player(profile.name,profile.avatar,profile.color);
		
		/* if the game doesnt exist, create it */
		if(!game) {
			data.games[gnum] = new Game(gnum);
			data.games[gnum].players[profile.name] = player;
			client.write("mazerace","games." + gnum,data.games[gnum]);
		}
		/* otherwise, store player info in game */
		else {
			data.games[gnum] = game;
			data.games[gnum].players[profile.name] = player;
			client.write("mazerace","games."+gnum+".players."+profile.name,player);
		}
		client.unlock("mazerace","games." + gnum);
		menu.enable("L");
		menu.enable("R");
		menu.enable("D");
		menu.enable("F");
		menu.disable("J");
		data.updated=true;
	}
	function getOpenGame() {
		for each(var g in data.games) {
			if(g.status == status.RACING)
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
	function leaveMaze() {
		delete data.games[gnum].players[profile.name];
		client.remove("mazerace","games."+gnum+".players."+profile.name,2);
		menu.disable("L");
		menu.disable("R");
		menu.disable("D");
		menu.disable("F");
		menu.enable("J");
		data.updated=true;
	}
	function toggleDamage() {
		data.games[gnum].damage = !data.games[gnum].damage;
		client.write("mazerace","games."+gnum+".damage",data.games[gnum].damage,2);
		data.updated=true;
	}
	function toggleFog() {
		data.games[gnum].fog = !data.games[gnum].fog;
		client.write("mazerace","games."+gnum+".fog",data.games[gnum].fog,2);
		data.updated=true;
	}
	function toggleReady() {
		var player = data.games[gnum].players[profile.name];
		player.ready = !player.ready;
		client.write("mazerace","games."+gnum+".players."+profile.name+".ready",player.ready,2);
		data.updated=true;
	}
	function redraw() {
		if(full_redraw) {
			frame.draw();
			full_redraw=false;
		}
		if(hotkeys)
			menu.draw();
		else
			input.draw();
		listPlayers();
	}

	open();
	main();
	close();
}
function race(gameNumber, profile)	{
	
	/* bitwise compass */
	const N=1;
	const E=2;
	const S=4;
	const W=8;
	
	/* mazeplay variables */
	var maze = data.mazes[gameNumber];
	var game = data.games[gameNumber];
	var player = maze.players[profile.name];

	/* display frames */
	var screen = new Frame(1,1,80,23,BG_BLACK | CYAN,frame);
	var fog = new Frame(1,1,80,23,BG_BLACK | RED,frame);
	var info = new Frame(1,24,80,1,BG_BLUE | YELLOW,frame);
	var start_time = Date.now();
	var end_time = undefined;
	
	/* main functions */
	function open() {
		client.subscribe(game_id,"mazes." + gameNumber);
		loadMaze();
		if(game.fog)
			loadFog();
		loadPlayers();
		screen.open();
		if(game.fog)
			fog.open();
		info.open();
		showPlayerInfo();
		clearFog(player);
	}
	function main()	{
		while(game.status != status.FINISHED) {
			cycle();
			var k=console.inkey(K_NOCRLF|K_NOECHO,5);
			switch(k.toUpperCase())	{
			case KEY_DOWN:
			case KEY_UP:
			case KEY_LEFT:
			case KEY_RIGHT:
				movePosition(k);
				if(playerDead()) {
					goToStart();
					showPlayerInfo();
				}
				else if(playerAtFinish()) {
					endGame();
				}
				break;
			case "Q":
			case "\x1b":
				return false;
			case "R":
				redraw();
			default:
				break;
			}
		}
		showWinner();
	}
	function close() {
		screen.close();
		fog.close();
		info.close();
		client.unsubscribe(game_id,"mazes." + maze.gameNumber);
	}
	function cycle() {
		client.cycle();
		while(client.updates.length > 0)
			processUpdate(client.updates.shift());
		if(frame.cycle())
			console.gotoxy(1,24);
	}
	function processUpdate(update) {
		var pName = undefined;
		try {
			switch(update.oper) {
			case "WRITE":
				var p=update.location.split(".");
				var obj=data;
				while(p.length > 1) {
					var child=p.shift();
					if(!obj[child])
						obj[child] = {};
					obj=obj[child];
					if(child.toUpperCase() == "PLAYERS")
						pName = p[0];
				}
				if(p[0].toUpperCase() == "COORDS") {
					var x = update.data.x+1;
					var y = update.data.y+1;
					maze.players[pName].frame.moveTo(x,y);
					clearFog(p);
				}
				else if(p[0].toUpperCase() == "HEALTH") {
					maze.players[pName].health = update.data;
					showPlayerInfo();
				}
				if(update.data == undefined)
					delete obj[p.shift()];
				else
					obj[p.shift()] = update.data;
				break;
			case "SUBSCRIBE":
				data.online=client.who(game_id,"games");
				break;
			case "UNSUBSCRIBE":
				data.online=client.who(game_id,"games");
				if(maze.players[update.data.nick])
					maze.players[update.data.nick].frame.delete();
				showPlayerInfo();
				break;
			}
		} catch(e) {
			log(LOG_ERROR,e);
			log(LOG_WARNING,update.toSource());
		}
	}

	/* helpers */
	function loadMaze() {
		var posx;
		var posy;
		var walls={
			NE:"\xC0",
			NW:"\xD9",
			SE:"\xDA",
			SW:"\xBF",
			I:"\xC5",
			V:"\xB3",
			H:"\xC4",
			TU:"\xC1",
			TD:"\xC2",
			TL:"\xB4",
			TR:"\xC3"
		};
		
		var r=0;
		for(;r<=settings.rows;r++) {
			posy = r*2;
			var c=0;
			for(;c<settings.columns;c++) {
				posx = c*3;
				var wall_state = N + E + S + W;
				if(!maze.maze[r] && c == 0) 
					wall_state &= ~W;
				else if(maze.maze[r] && (!maze.maze[r][c-1] || !(maze.maze[r][c-1] & N)))
					wall_state &= ~W;
				if(!maze.maze[r-1] || !(maze.maze[r-1][c] & W))
					wall_state &= ~N;
				if(maze.maze[r] && (maze.maze[r][c] && !(maze.maze[r][c] & N)))
					wall_state &= ~E;
				if(!maze.maze[r] || !(maze.maze[r][c] & W))
					wall_state &= ~S;
									
				switch(wall_state) {
					case (N+S+E+W):
						screen.setData(posx,posy,walls.I);
						break;
					case (N+S+E):
						screen.setData(posx,posy,walls.TR);
						break;
					case (N+S+W):
						screen.setData(posx,posy,walls.TL);
						break;
					case (N+E+W):
						screen.setData(posx,posy,walls.TU);
						break;
					case (S+E+W):
						screen.setData(posx,posy,walls.TD);
						break;
					case (N+S):
						screen.setData(posx,posy,walls.V);
						break;
					case (E+W):
						screen.setData(posx,posy,walls.H);
						break;
					case (N+E):
						screen.setData(posx,posy,walls.NE);
						break;
					case (N+W):
						screen.setData(posx,posy,walls.NW);
						break;
					case (S+E):
						screen.setData(posx,posy,walls.SE);
						break;
					case (S+W):
						screen.setData(posx,posy,walls.SW);
						break;
					case S:
					case N:
						screen.setData(posx,posy,walls.V);
						break;
					case E:
					case W:
						screen.setData(posx,posy,walls.H);
						break
				}
				if(wall_state & E) {
					screen.setData(posx+1,posy,walls.H);
					screen.setData(posx+2,posy,walls.H);
				}
				if(wall_state & S) {
					screen.setData(posx,posy+1,walls.V);
				}
			}
			if(r==0)
				screen.setData(posx+3,posy,walls.SW);
			else if(r == settings.rows)
				screen.setData(posx+3,posy,walls.NW);
			else if(maze.maze[r] && maze.maze[r][c-1] & N)
				screen.setData(posx+3,posy,walls.TL);
			else 
				screen.setData(posx+3,posy,walls.V);
			if(maze.maze[r])
				screen.setData(posx+3,posy+1,walls.V);
		}
		screen.setData(maze.start.x*3+1,maze.start.y*2+1,"\xAF",LIGHTGREEN);
		screen.setData(maze.finish.x*3+2,maze.finish.y*2+1,"\xAF",LIGHTRED);
	}
	function loadFog() {
		fog.load(root + "fog.bin",80,23);
		fog.transparent = true;
	}
	function loadPlayers() {
		var x = maze.start.x*3+1;
		var y = maze.start.y*2+1;
		for each(var p in maze.players) {
			p.coords = new Coords(x,y);
			p.color = settings.colors.shift();
			p.avatar = settings.avatars.shift();
			p.health = settings.max_health;
			p.frame = new Frame(x+1,y+1,1,1,p.color,screen);
			p.frame.putmsg(p.avatar)
			settings.colors.push(p.color);
			settings.avatars.push(p.avatar);
		}
	}
	function endGame() {
	
		if(countPlayers() > 1) {
			game.winner = player.name;
			data.storeRaceWinner(gameNumber,game.winner);
		}
		if(game.fog)
			fog.close();
		end_time = Date.now();
		game.raceTime = end_time - start_time;
		data.storeRaceTime(gameNumber,game.raceTime,player.name);
		data.storeGameStatus(gameNumber,status.FINISHED);
	}
	function showWinner() {
		var winFrame = new Frame(25,9,30,6,BG_BLUE + YELLOW,frame);
		winFrame.open();
		winFrame.crlf();
		winFrame.center("\1r\1hGAME OVER\r\n");
		if(game.winner)
			winFrame.center("\1y\1hWinner: " + game.winner + "\r\n");
		winFrame.center("\1w\1hTime: " + formatTime(game.raceTime) + "\r\n");
		winFrame.crlf();
		winFrame.center("\1c\1h<SPACE to continue>");
		winFrame.draw();
		while(console.getkey(K_NOCRLF|K_NOECHO) !== " ");
		winFrame.close();
	}
	function redraw() {
		screen.draw();
		fog.draw();
	}
	function showPlayerInfo() {
		info.clear();
		for each(var p in maze.players) {
			var ph = p.health/100;
			var bars = Math.floor(ph * 7);
			var str = " " + getColor(p.color) + p.avatar + ": ";
			if(ph > .75)
				str+="\1g\1h";
			else if(ph > .50)
				str+="\1y\1h";
			else
				str+="\1r\1h";
			var bstr = "";
			for(var b=0;b<bars;b++)
				bstr+="\xDB";
			if(ph*7%1 >= .5)
				bstr+="\xDD";
			str+=format("%-*s",8,bstr);	
			info.putmsg(str);
		}
	}
	function countPlayers() {
		var count=0;
		for each(var p in maze.players) {
			count++;
		}
		return count;
	}

	/* movement */
	function checkMove(x,y) {
		if(!screen.getData(x-1,y-1).ch || screen.getData(x-1,y-1).ch == "\xAF")
			return false;
		else
			return true;
	}
	function movePlayer(x,y) {
		player.coords.x += x;
		player.coords.y += y;
		player.frame.move(x,y);
		clearFog(player);
		data.storePlayerPosition(gameNumber,player);
	}
	function clearFog(p) {
		if(game.fog) {
			for(var x=p.coords.x-3;x<p.coords.x+3;x++) {
				for(var y=p.coords.y-2;y<p.coords.y+2;y++) {
					fog.clearData(x,y);
				}
			}
		}
	}
	function takeDamage() {
		if(game.damage) {
			player.health -= settings.damage_qty;
			write(ascii(7));
			data.storePlayerHealth(gameNumber,player);
			showPlayerInfo();
		}
	}
	function playerDead() {
		if(player.health <= 0) 
			return true;
		return false;
	}
	function goToStart() {
		var x = maze.start.x*3+1;
		var y = maze.start.y*2+1;
		player.coords = new Coords(x,y);
		player.health = settings.max_health;
		data.storePlayerPosition(gameNumber,player);
		data.storePlayerHealth(gameNumber,player);
		player.frame.moveTo(x+1,y+1);
	}
	function playerAtFinish() {
		if(player.coords.x == (maze.finish.x*3+2) && player.coords.y == (maze.finish.y*2+1)) 
			return true;
		return false;
	}
	function movePosition(k) {
		switch(k) {
		case KEY_DOWN:
			if(checkMove(player.frame.x,player.frame.y+1)) 
				takeDamage(); 
			else
				movePlayer(0,1);
			break;
		case KEY_UP:
			if(checkMove(player.frame.x,player.frame.y-1)) 
				takeDamage(player); 
			else
				movePlayer(0,-1);
			break;
		case KEY_LEFT:
			if(checkMove(player.frame.x-1,player.frame.y)) 
				takeDamage(player); 
			else
				movePlayer(-1,0);
			break;
		case KEY_RIGHT:
			if(checkMove(player.frame.x+1,player.frame.y)) 
				takeDamage(player); 
			else
				movePlayer(1,0);
			break;
		default:
			return;
		}
	}

	open();
	main();
	close();
}

open();
lobby();
close();
