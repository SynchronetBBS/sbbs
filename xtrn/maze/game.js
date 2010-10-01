/*
	SYNCHRONET MAZE RACE 
	A Javascript remake 
	of the Atari game "Maze Craze"

	For Synchronet v3.15+
	Matt Johnson(2008)
*/
//$Id$
const VERSION="$Revision$".split(' ')[1];

load("graphic.js");
load("logging.js");
load("chateng.js");

var oldpass=console.ctrlkey_passthru;
var root=js.exec_dir;

load(root + "mazegen.js");
load(root + "mazeobj.js");
load(root + "timer.js");
load(root + "menu.js");

var stream=argv[0];
var chat=new ChatEngine();
var generator=new MazeGenerator(10,26);
var players=new PlayerList();
var scores=new ScoreList();

function splashStart()
{
	console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
	bbs.sys_status|=SS_MOFF;
	bbs.sys_status|=SS_PAUSEOFF;
	console.clear();
	//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
}
function splashExit()
{
	//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
	console.ctrlkey_passthru=oldpass;
	bbs.sys_status&=~SS_MOFF;
	bbs.sys_status&=~SS_PAUSEOFF;
	console.clear(ANSI_NORMAL);
	sendFiles("players.ini");
	
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
	console.getkey(K_NOSPIN|K_NOECHO);
	console.clear(ANSI_NORMAL);
}
function lobby()
{
	var background=new Graphic(80,24);

	var menu;
	var mazes=[];
	var update=false;
	
	function init()
	{
		updateMazes();
		initChat();
		initMenu();
		background.load(root + "lobby.bin");
		notice("\1c\1hWelcome to Maze Race!");
		notice("\1n\1ctype '/' for a list of available menu commands,");
		notice("\1n\1cor you can just start typing to chat.");
		notice("\1n\1cPress 'Escape' to quit.");
		notice(" ");
	}
	function initChat()
	{
		chat.init(56,18,2,4);
		chat.input_line.init(2,23,56,"","\1n");
		chat.joinChan("maze race lobby",user.alias,user.name);
	}
	function initMenu()
	{
		var menu_items=[		"~Start New Race"				, 
								"~Join Race"					,
								"~Rankings"						,
								"~Help"							,
								"Re~draw"						];
		menu=new Menu(menu_items,"\1n","\1c\1h");
	}
	function main()
	{
		redraw();
		while(1) {
			cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k) {
				switch(k.toUpperCase()) {
					case "/":
						if(!chat.input_line.buffer.length) {
							refreshCommands();
							listCommands();
							getMenuCommand();
							redraw();
						}
						else if(!Chat(k,chat)) return;
						break;
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
		updateMazes();

		for each(var m in mazes) {
			switch(Number(m.status)) {
			case -1:
				if(countMembers(m.players)>1) {
					initTimer(m);
					update=true;
				}
				break;
			case 0:
				if(!m.timer.countdown) m.loadData();
				var difference=time()-m.timer.lastupdate;
				if(!difference>=1) break;
				if(!m.timer.countDown()) {
					startRace(m);
				}
				update=true;
				break;
			case 1:
				var id=players.getPlayerID(user.alias);
				if(m.players[id]) {
					race(m);
					redraw();
				}
				break;
			case 2:
				break;
			default:
				log("unknown game status: " + m.status);
				mswait(500);
				break;
			}
		}
		if(update) {
			listMazes();
			update=false;
		}
		mswait(5);
	}
	function initTimer(maze)
	{
		maze.status=0;
		var file=new File(maze.dataFile);
		file.open('r+',true);
		file.iniSetValue(null,"created","" + time());
		file.iniSetValue(null,"status",0);
		file.close();
	}
	function startRace(maze)
	{
		log("starting race: " + maze.gameNumber);
		maze.status=1;
		var file=new File(maze.dataFile);
		file.open('r+',true);
		file.iniSetValue(null,"status",1);
		file.close();
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
	function help()
	{
	
	}
	function notice(txt)
	{
		chat.chatroom.notice(txt);
	}
	function refreshCommands()
	{
		if(countMembers(mazes)>0) {
			menu.enable(["J"]);
		} else {
			menu.disable(["J"]);
		}
	}
	function showRankings()
	{
	}
	function getMenuCommand()
	{
		while(1) {
			var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
			if(k) 	{
				switch(k.toUpperCase())
				{
				case "R":
					showRankings();
					break;
				case "H":
					help();
					break;
				case "S":
					createMaze();
					break;
				case "J":
					selectMaze();
					break;
				default:
					break;
				}
				return true;
			}
		}
	}
	function selectMaze()
	{
		var gameNumber=menuPrompt("\1nEnter maze #\1h: ");
		if(!mazes[gameNumber]) {
			notify("No such maze!");
			return false;
		}
		joinMaze(mazes[gameNumber]);
		return true;
	}
	function createMaze()
	{
		var id=players.getPlayerID(user.alias);
		for each(var m in mazes) {
			if(m.players[id]) {
				menuPrompt("\1r\1hYou are already in a game \1n\1r[\1hpress a key\1n\1r]");
				return false;
			}
		}
		var gameNumber=getNewGameNumber();
		var rootFileName=getRootFileName(gameNumber);
		var maze=new Maze(rootFileName,gameNumber);
		maze.players[id]=new Player(id,maze.colors.shift(),100);
		maze.goToStartingPosition(maze.players[id]);
		mazes[maze.gameNumber]=maze;

		sendFiles(maze.mazeFile);
		storeGameData(maze);
	}
	function joinMaze(maze)
	{
		var id=players.getPlayerID(user.alias);
		if(maze.players[id]) {
			menuPrompt("\1r\1hYou are already in that game \1n\1r[\1hpress a key\1n\1r]");
			return false;
		}
		if(countMembers(maze.players) == maze.colors.length) {
			menuPrompt("\1r\1hThat game is full \1n\1r[\1hpress a key\1n\1r]");
			return false;
		}
		maze.players[id]=new Player(id,maze.colors.shift(),100);
		maze.goToStartingPosition(maze.players[id]);
		storePlayerData(maze,id);
		notice("You joined maze #" + maze.gameNumber);
	}	
	function updateMazes()
	{
		var maze_files=directory(root+"maze*.ini");
		for(var i=0;i<maze_files.length;i++) {
			var filename=file_getname(maze_files[i]);
			var gameNumber=Number(filename.substring(4,filename.indexOf(".")));
			
			if(mazes[gameNumber]) {
				var lastupdate=file_date(maze_files[i]);
				var lastloaded=mazes[gameNumber].lastupdate;
				if(lastupdate>lastloaded) {
					log("Updating maze: " +  maze_files[i]);
					log("last update: " + lastloaded);
					mswait(500);
					mazes[gameNumber].loadData();
					update=true;
				}
			} else {
				log("loading maze: " + maze_files[i]);
				mazes[gameNumber]=loadMaze(gameNumber);
				update=true;
			}
		}
		for(m in mazes) {
			var maze=mazes[m];
			if(maze && !file_exists(maze.dataFile)) {
				log("removing deleted maze: " + maze.dataFile);
				delete mazes[m];
				update=true;
			}
		}
	}
	function loadMaze(gameNumber)
	{
		var rootFileName=root + "maze" + gameNumber;
		return new Maze(rootFileName,gameNumber);
	}
	function listMazes()
	{
		var ip=new Coords(60,6);
		clearBlock(60,6,19,7);
		var wp=new Coords(60,16);
		clearBlock(60,16,19,7);
		var in_progress=[];
		var waiting=[];
		
		for(var m in mazes) {
			var maze=mazes[m];
			if(maze.status == 1)
				in_progress.push(maze);
			else 
				waiting.push(maze);
		}
		
		console.gotoxy(ip);
		console.pushxy();
		for each(var m in in_progress) {
			console.putmsg("\1n\1cMaze #" + m.gameNumber);
			console.popxy();
			console.down();
			console.pushxy();
		}
		console.gotoxy(wp);
		console.pushxy();
		for each(var m in waiting) {
			console.putmsg("\1n\1cMaze #\1h" + m.gameNumber);
			if(m.timer.countdown > 0)
				console.putmsg(" \1n\1c: \1r\1h" + parseInt(m.timer.countdown,10));
			console.popxy();
			console.down();
			console.pushxy();
		}
	}
	function redraw()
	{
		background.draw();
		chat.redraw();
		listMazes();
	}

	init();
	main();
}
function race(maze) 
{
	var currentPlayerID=players.getPlayerID(user.alias);
	var player=maze.players[currentPlayerID];
	var update=true;
	
	function init()
	{
		maze.draw();
		showPlayerInfo();
	}
	function main()
	{
		while(!maze.winner) {
			cycle();
			var k=console.inkey();
			switch(k.toUpperCase())
			{
			case KEY_DOWN:
			case KEY_UP:
			case KEY_LEFT:
			case KEY_RIGHT:
				movePosition(k);
				break;
			case "Q":
			case "\x1b":
				deleteMaze();
				return false;
			case "R":
				redraw();
			default:
				break;
			}
		}
		deleteMaze();
		console.home();
		console.cleartoeol();
		console.putmsg("\1y\1h" + maze.winner + " has won! \1r[press 'Q' to return to the lobby]");
		return(console.getkeys('Q'));
	}
	function processData(packet)
	{
		if(packet.gameNumber != maze.gameNumber) return false;
		switch(packet.func.toUpperCase()) {
		case "MOVE":
			var p=packet.player;
			var coords=packet.coords;
			var health=packet.health;
			
			maze.players[p].unDraw();
			if(maze.players[p].coords.x == player.coords.x && maze.players[p].coords.y == player.coords.y) {
				player.draw();
				send();
			}
			maze.players[p].coords=coords;
			maze.players[p].health=health;
			maze.players[p].draw();
			showPlayerInfo();
			checkWinner();
			break;
		default:
			log("Unknown chess data type received");
			log("packet: " + packet.toSource());
			break;
		}
	}
	function cycle()
	{
		var packet=stream.receive();
		if(packet)	processData(packet);
		
		if(update) {
			send();
			player.draw();
			checkWinner();
			update=false;
		}
	}
	function movePosition(k)
	{
		switch(k)
		{
		case KEY_DOWN:
			if(checkMove(player.coords.x,player.coords.y+1)) {
				kk=false; 
				showPlayerInfo(); 
				break;
			}
			player.unDraw();
			player.coords.y++;
			update=true;
			break;
		case KEY_UP:
			if(checkMove(player.coords.x,player.coords.y-1)) {
				kk=false; 
				showPlayerInfo(); 
				break;
			}
			player.unDraw();
			player.coords.y--;
			update=true;
			break;
		case KEY_LEFT:
			if(checkMove(player.coords.x-1,player.coords.y)) {
				kk=false; 
				showPlayerInfo(); 
				break;
			}
			player.unDraw();
			player.coords.x--;
			update=true;
			break;
		case KEY_RIGHT:
			if(checkMove(player.coords.x+1,player.coords.y)) {
				kk=false; 
				showPlayerInfo(); 
				break;
			}
			player.unDraw();
			player.coords.x++;
			update=true;
			break;
		}
	}
	function redraw()
	{
		DrawMaze();
		showPlayerInfo();
		for(ply in players) {
			players[ply].draw();
		}
		console.center("\1k\1hUse Arrow Keys to Move. First player to reach '\1yX\1k' wins. - [\1cQ\1k]uit [\1cR\1k]edraw");
	}
	function checkMove(posx,posy)
	{
		var data=maze.maze.data;
		/*for each(var p in maze.players) {
			if(posx == p.coords.x && posy == p.coords.y) {
				return true;
			} 
		}*/
		if(	data[posx-1][posy-1].ch==" " || data[posx-1][posy-1].ch=="S" || data[posx-1][posy-1].ch=="X") {
			return false;
		}
		if(maze.damage) {
			player.health-=5;
			update=true;
		}
		if(player.health==0) {
			player.unDraw();
			maze.goToStartingPosition(player);
			player.health=100;
		}
		return true;
	}
	function showPlayerInfo()
	{
		console.home();
		for(var p in maze.players) {
			var ply=maze.players[p];
			console.putmsg(" " + ply.color + ply.name + "\1k\1h:" + (ply.health<=25?"\1r":ply.color) + ply.health);
		}
		console.cleartoeol();
	}
	function deleteMaze()
	{
		delete maze.players[currentPlayerID];
		if(file_exists(maze.mazeFile)) {
			file_remove(maze.mazeFile);
		}
		if(file_exists(maze.mazeFile + ".bck")) {
			file_remove(maze.mazeFile + ".bck");
		}
		if(file_exists(maze.dataFile)) {
			file_remove(maze.dataFile);
		}
		if(file_exists(maze.dataFile + ".bck")) {
			file_remove(maze.dataFile + ".bck");
		}
	}
	function send()
	{
		var data=new Object;
		data.player=currentPlayerID;
		data.coords=player.coords;
		data.health=player.health;
		data.func="MOVE";
		data.gameNumber=maze.gameNumber;
		stream.send(data);
	}
	function checkWinner()
	{
		for each(var p in maze.players) {
			if(p.coords.x == maze.finish.x && p.coords.y == maze.finish.y) {
				maze.winner=p.name;
				return true;
			}
		}
		return false;
	}
	
	init();
	main();
}
function getRootFileName(gameNumber)
{
	return(root + "maze" + gameNumber);
}
function getNewGameNumber()
{
	var gNum=1;
	while(file_exists(root + "maze" + gNum + ".ini")) {
		gNum++;
	}
	return gNum;
}
function menuPrompt(text)
{
	chat.input_line.clear();
	console.gotoxy(chat.input_line);
	console.putmsg(text);
	var key=console.getkey();
	chat.input_line.draw();
	return key;
}
function notify(message)
{
	chat.chatroom.alert(message);
}
function notice(message)
{
	chat.chatroom.notice(message);
}
function storeGameData(maze)
{
	//STORE GAME DATA
	var file=new File(maze.dataFile+".tmp");
	file.open("w+");
	
	file.iniSetValue(null,"gameNumber",maze.gameNumber);
	file.iniSetValue(null,"status",maze.status);
	
	if(maze.created) file.iniSetValue(null,"created",maze.created);
	
	for(var p in maze.players) {
		file.iniSetValue("players",p,"");
	}

	file.close();
	file_remove(maze.dataFile);
	file_rename(file.name,maze.dataFile);
	sendFiles(maze.dataFile);
}
function storePlayerData(maze,player)
{
	var file=new File(maze.dataFile);
	file.open('r+',true);
	file.iniSetValue("players",player);
	file.close();
	sendFiles(maze.dataFile);
}
function sendFiles(mask)
{
	stream.sendfile(mask);
}

splashStart();
lobby();
splashExit();
