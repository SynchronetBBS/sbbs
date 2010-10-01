/*
	-----------------------------------------------------------
	Tank Battle! - Javascript Module for Synchronet 3.14+
	By Matt Johnson - MCMLXXIX - (2008) 
	-----------------------------------------------------------
	The Broken Bubble (bbs.thebrokenbubble.com)
	-----------------------------------------------------------
	Home of this and many other original javascript games
	for Synchronet BBS systems.
	-----------------------------------------------------------
*/
//$Id$
const VERSION="$Revision$".split(' ')[1];

var root=js.exec_dir;

load("graphic.js");
load("sbbsdefs.js");
load("chateng.js");

load(root + "timer.js");
load(root + "menu.js");
load(root + "tankobj.js");

var oldpass=console.ctrlkey_passthru;
var stream=argv[0];
var chat=new ChatEngine(root);
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
	if(!file_exists(splash_filename)) exit();
	
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
	return;
}
function lobby()
{
	var background=new Graphic(80,24);
	var menu;
	var battles=[];
	var update=false;
	
	function init()
	{
		updateBattles();
		initChat();
		initMenu();
		background.load(root + "lobby.bin");
		notice("\1c\1hWelcome to Tank Battle!");
		notice("\1n\1ctype '/' for a list of available menu commands,");
		notice("\1n\1cor you can just start typing to chat.");
		notice("\1n\1cPress 'Escape' to quit.");
		notice(" ");
	}
	function initChat()
	{
		chat.init(56,18,2,4);
		chat.input_line.init(2,23,56,"","\1n");
		chat.joinChan("tank battle lobby",user.alias,user.name);
	}
	function initMenu()
	{
		var menu_items=[		"~Start New Battle"				, 
								"~Join Battle"					,
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
		updateBattles();

		for each(var b in battles) {
			switch(Number(b.status)) {
			case -1:
				if(countMembers(b.players)>1) {
					initTimer(b);
					update=true;
				}
				break;
			case 0:
				if(!b.timer.countdown) b.loadData();
				var difference=time()-b.timer.lastupdate;
				if(!difference>=1) break;
				if(!b.timer.countDown()) {
					startGame(b);
				}
				update=true;
				break;
			case 1:
				var id=players.getPlayerID(user.alias);
				if(b.players[id]) {
					playGame(b);
					redraw();
				}
				break;
			case 2:
				break;
			default:
				log("unknown game status: " + b.status);
				mswait(500);
				break;
			}
		}
		if(update) {
			listBattles();
			update=false;
		}
		mswait(5);
	}
	function initTimer(battle)
	{
		log("starting timer for battle: " + battle.gameNumber);
		battle.status=0;
		var file=new File(battle.dataFile);
		file.open('r+',true);
		file.iniSetValue(null,"created","" + time());
		file.iniSetValue(null,"status",0);
		file.close();
	}
	function startGame(battle)
	{
		log("starting game: " + battle.gameNumber);
		battle.status=1;
		storeGame(battle);
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
		if(countMembers(battles)>0) {
			menu.enable(["J"]);
		} else {
			menu.disable(["J"]);
		}
	}
	function showRankings()
	{
	}
	function getMapList()
	{
		var maplist=directory(root + "map_*.bin");
		return maplist;
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
					createBattle();
					break;
				case "J":
					selectBattle();
					break;
				default:
					break;
				}
				return true;
			}
		}
	}
	function selectBattle()
	{
		var gameNumber=menuPrompt("\1nEnter battle #\1h: ");
		if(!battles[gameNumber]) {
			notify("No such battle!");
			return false;
		}
		joinBattle(battles[gameNumber]);
		return true;
	}
	function createBattle()
	{
		var id=players.getPlayerID(user.alias);
		for each(var b in battles) {
			if(b.players[id]) {
				menuPrompt("\1r\1hYou are already in a game \1n\1r[\1hpress a key\1n\1r]");
				return false;
			}
		}
	
		var list=getMapList();
		chat.chatroom.clear();
		console.gotoxy(chat.chatroom);
		console.pushxy();
		for(l=0;l<list.length;l++) {
			var fname=file_getname(list[l]).split(".")[0];
			var mapname=fname.substr(fname.indexOf("_")).replace(/_/g," ");
			console.putmsg("\1n" + (l+1) + "\1h: \1n\1c" + mapname);
			console.popxy();
			console.down();
			console.pushxy();
		}
		var mapNumber=menuPrompt("\1nChoose a map: ");
		if(!list[mapNumber-1]) {
			menuPrompt("\1r\1hNo such map! \1n\1r[\1hpress a key\1n\1r]");
			return false;
		}
		
		var mapFile=list[mapNumber-1];
		var battle=new Battle(false,mapFile);
		
		var position=0;
		var player=new Player(id,position,100);
		player.start=battle.start[position];
		player.color=battle.color[position];
		player.coords=new Coords(player.start.x,player.start.y);
		battle.players[id]=player;
		
		notice("\1g\1hGame #" + parseInt(battle.gameNumber,10) + " created");
		storeGame(battle);
	}
	function joinBattle(battle)
	{
		var id=players.getPlayerID(user.alias);
		if(battle.players[id]) {
			menuPrompt("\1r\1hYou are already in that game \1n\1r[\1hpress a key\1n\1r]");
			return false;
		}
		if(countMembers(battle.players) == battle.start.length) {
			menuPrompt("\1r\1hThat game is full \1n\1r[\1hpress a key\1n\1r]");
			return false;
		}
		var position=countMembers(battle.players);
		var player=new Player(id,position,100);
		player.start=battle.start[position];
		player.color=battle.color[position];
		player.coords=new Coords(player.start.x,player.start.y);
		battle.players[id]=player;
		storePlayerData(battle,id,position);
		notice("You joined battle #" + battle.gameNumber);
	}	
	function updateBattles()
	{
		var battle_files=directory(root+"battle*.ini");
		for(var i=0;i<battle_files.length;i++) {
			var filename=file_getname(battle_files[i]);
			var gameNumber=Number(filename.substring(6,filename.indexOf(".")));
			
			if(battles[gameNumber]) {
				var lastupdate=file_date(battle_files[i]);
				var lastloaded=battles[gameNumber].lastupdate;
				if(lastupdate>lastloaded) {
					log("Updating battle: " +  battle_files[i]);
					battles[gameNumber].loadData();
					update=true;
				}
			} else {
				log("loading battle: " + battle_files[i]);
				loadBattle(battle_files[i]);
				update=true;
			}
		}
		for(m in battles) {
			var battle=battles[m];
			if(battle && !file_exists(battle.dataFile)) {
				log("removing deleted battle: " + battle.dataFile);
				delete battles[m];
				update=true;
			}
		}
	}
	function loadBattle(dataFile)
	{
		var battle=new Battle(dataFile);
		log("loaded battle: " + battle.gameNumber);
		battles[battle.gameNumber]=battle;
	}
	function storePlayerData(battle,id,position)
	{
		var file=new File(battle.dataFile);
		file.open('r+',true);
		file.iniSetValue("players",id,position);
		file.close();
	}
	function listBattles()
	{
		var ip=new Coords(60,6);
		clearBlock(60,6,19,7);
		var wp=new Coords(60,16);
		clearBlock(60,16,19,7);
		var in_progress=[];
		var waiting=[];
		
		for(var m in battles) {
			var battle=battles[m];
			if(battle.status == 1)
				in_progress.push(battle);
			else 
				waiting.push(battle);
		}
		
		console.gotoxy(ip);
		console.pushxy();
		for each(var m in in_progress) {
			console.putmsg("\1n\1cBattle #" + m.gameNumber);
			console.popxy();
			console.down();
			console.pushxy();
		}
		console.gotoxy(wp);
		console.pushxy();
		for each(var m in waiting) {
			console.putmsg("\1n\1cBattle #\1h" + m.gameNumber);
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
		listBattles();
	}

	init();
	main();
}
function playGame(battle) 
{
	var currentPlayerID=players.getPlayerID(user.alias);
	var currentPlayer=battle.players[currentPlayerID];
	var tankCompass=[0,90];
	var turretCompass=[0,45,90,135,180,225,270,315];
	var shots=[];

	/* ms bulletTick between cycles */
	var bulletTick=.05; 
	var shotTick=1;
	var shotRange=40;
	var lastUpdate=-1;
	var lastShot=-1;
	
	/* main functions */
	function init()
	{
		battle.draw();
		showPlayerInfo();
		for each(var p in battle.players) {
			drawTank(p);
			drawTurret(p);
		}
	}
	function main()
	{
		while(1) {
			cycle();
			var k=console.inkey();
			switch(k.toUpperCase())
			{
			case KEY_DOWN:
				moveDown();
				break;
			case KEY_UP:
				moveUp();
				break;
			case KEY_LEFT:
				moveLeft();
				break;
			case KEY_RIGHT:
				moveRight();
				break;
			case " ":
				if(system.timer - lastShot >= shotTick)
					fireShot();
				break;
			case "A":
			case "D":
				rotateTurret(k);
				break;
			case "Q":
			case "\x1b":
				deleteBattle();
				return false;
			case "R":
				redraw();
			default:
				break;
			}
		}
		deleteBattle();
		console.home();
		console.cleartoeol();
		return(console.getkeys('Q'));
	}
	function cycle()
	{
		var packet=stream.receive();
		if(packet)	processData(packet);
		
		var difference=system.timer-lastUpdate;
		if(difference < bulletTick) return;
				
		for(var s=0;s<shots.length;s++) {
			var shot=shots[s];

			unDrawShot(shot);
			if(shot.range == 0) {
				shots.splice(s,1);
				s--;
				console.home();
				continue;
			} 
			moveShot(shot);
			
			var hit=false;
			for(var p in battle.players) {
				var player=battle.players[p];
				if(checkHit(shot,player)) {
					hit=true;
					shots.splice(s,1);
					s--;
					if(p == currentPlayerID) {
						hitPlayer(shot.range);
					}
					break;
				} 
			}
			if(!hit) {
				drawShot(shot);
			}			
		}
		lastUpdate=system.timer;
	}

	/* data functions */
	function processData(packet)
	{
		if(packet.gameNumber != battle.gameNumber) return false;
		switch(packet.func.toUpperCase()) {
		case "TANK":
			var p=packet.player;
			unDrawTank(battle.players[p]);
			
			battle.players[p].coords=packet.coords;
			battle.players[p].health=packet.health;
			battle.players[p].heading=packet.heading;
			battle.players[p].turret=packet.turret;
			
			drawTank(battle.players[p]);
			drawTurret(battle.players[p]);
			showPlayerInfo();
			break;
		case "HEALTH":
			var p=packet.player;
			battle.players[p].health=packet.health;
			break;
		case "SPLODE":
			var p=packet.player;
			killPlayer(battle.players[p]);
			break;
		case "TURRET":
			var p=packet.player;
			battle.players[p].turret=packet.turret;
			drawTank(battle.players[p]);
			drawTurret(battle.players[p]);
			break;
		case "SHOT":
			var shot=new Shot(packet.heading,packet.coords,packet.range,packet.player,packet.color);
			var p=packet.player;
			drawShot(shot);
			shots.push(shot);
			break;
		default:
			log("Unknown tank battle data received");
			log("packet: " + packet.toSource());
			break;
		}
	}
	function packageData(func)
	{
		var data=new Packet(func);
		switch(func)
		{
		case "SHOT":
			data.heading=turretCompass[currentPlayer.turret];
			data.coords=getShotCoords(data.heading,currentPlayer.coords.x,currentPlayer.coords.y);
			data.range=shotRange;
			data.color=currentPlayer.color;
			break;
		case "HEALTH":
			data.health=currentPlayer.health;
			break;
		case "SPLODE":
			/* no additional data necessary */
			break
		case "TURRET":
			data.turret=currentPlayer.turret;
			break;
		case "TANK":
			data.coords=currentPlayer.coords;
			data.health=currentPlayer.health;
			data.heading=currentPlayer.heading;
			data.turret=currentPlayer.turret;
			break;
		}
		data.gameNumber=battle.gameNumber;
		data.player=currentPlayerID;
		return data;
	}
	function deleteBattle()
	{
		delete battle.players[currentPlayerID];
		if(file_exists(battle.dataFile)) {
			file_remove(battle.dataFile);
		}
		if(file_exists(battle.dataFile + ".bck")) {
			file_remove(battle.dataFile + ".bck");
		}
	}
	function send(func)
	{
		var data=packageData(func);
		stream.send(data);
	}
	
	/* tank functions */
	function rotateTurret(direction)
	{
		// LEFT
		if(direction.toUpperCase()=="A") {
			if(currentPlayer.turret==0) currentPlayer.turret=turretCompass.length-1;
			else currentPlayer.turret--;
		}
		// RIGHT
		if(direction.toUpperCase()=="D") {
			if(currentPlayer.turret==turretCompass.length-1) currentPlayer.turret=0;
			else currentPlayer.turret++;
		}
		drawTank(currentPlayer);
		drawTurret(currentPlayer);
		send("TURRET");
	}
	function moveDown()
	{
		if(currentPlayer.heading != 0) {
			currentPlayer.heading = 0;
			drawTank(currentPlayer);
			drawTurret(currentPlayer);
			send("TANK");
			return;
		}
		
		var test_coords=new Coords(currentPlayer.coords.x,currentPlayer.coords.y+1);
		if(checkWalls(test_coords)) {
			log("wall interference found");
			return false;
		}
		
		unDrawTank(currentPlayer);
		currentPlayer.coords.y++;
		drawTank(currentPlayer);
		drawTurret(currentPlayer);
		send("TANK");
		return true;
	}
	function moveUp()
	{
		if(currentPlayer.heading != 0) {
			currentPlayer.heading = 0;
			drawTank(currentPlayer);
			drawTurret(currentPlayer);
			send("TANK");
			return;
		}
		
		var test_coords=new Coords(currentPlayer.coords.x,currentPlayer.coords.y-1);
		if(checkWalls(test_coords)) {
			log("wall interference found");
			return false;
		}
		
		unDrawTank(currentPlayer);
		currentPlayer.coords.y--;
		drawTank(currentPlayer);
		drawTurret(currentPlayer);
		send("TANK");
		return true;
	}
	function moveLeft()
	{
		if(currentPlayer.heading != 90) {
			currentPlayer.heading = 90;
			drawTank(currentPlayer);
			drawTurret(currentPlayer);
			send("TANK");
			return;
		}
		
		var test_coords=new Coords(currentPlayer.coords.x-1,currentPlayer.coords.y);
		if(checkWalls(test_coords)) {
			log("wall interference found");
			return false;
		}
		
		unDrawTank(currentPlayer);
		currentPlayer.coords.x--;
		drawTank(currentPlayer);
		drawTurret(currentPlayer);
		send("TANK");
		return true;
	}
	function moveRight()
	{
		if(currentPlayer.heading != 90) {
			currentPlayer.heading = 90;
			drawTank(currentPlayer);
			drawTurret(currentPlayer);
			send("TANK");
			return;
		}
		
		var test_coords=new Coords(currentPlayer.coords.x+1,currentPlayer.coords.y);
		if(checkWalls(test_coords)) {
			log("wall interference found");
			return false;
		}
		
		unDrawTank(currentPlayer);
		currentPlayer.coords.x++;
		drawTank(currentPlayer);
		drawTurret(currentPlayer);
		send("TANK");
		return true;
	}
	function fireShot()
	{
		lastShot=system.timer;
		var heading=turretCompass[currentPlayer.turret];
		var coords=getShotCoords(heading,currentPlayer.coords.x,currentPlayer.coords.y);
		if(checkPosition(coords.x,coords.y)) {
			hitPlayer(25);
			return;
		}
		var shot=new Shot(heading,coords,shotRange,currentPlayerID,currentPlayer.color);
		drawShot(shot);
		shots.push(shot);
		send("SHOT");
	}
	function hitPlayer(damage)
	{
		var startHealth=currentPlayer.health;
		currentPlayer.health-=(damage);
		
		if(currentPlayer.health<=0) {
			send("SPLODE");
			killPlayer(currentPlayer);
			resetPosition();
		} else if(currentPlayer.health <= 25 && startHealth > 25) {
			drawTank(currentPlayer);
			drawTurret(currentPlayer);
			send("TANK");
		} else if(currentPlayer.health <= 50 && startHealth > 50) {
			drawTank(currentPlayer);
			drawTurret(currentPlayer);
			send("TANK");
		} else {
			send("HEALTH");
		}
		showPlayerInfo();
	}
	function getShotCoords(heading,x,y)
	{
		switch(heading) {
		case 0:
			y-=2;
			break;
		case 45:
			y-=2;
			x+=2;
			break;
		case 90:
			x+=2;
			break;
		case 135:
			x+=2;
			y+=2;
			break;
		case 180:
			y+=2;
			break;
		case 225:
			x-=2;
			y+=2;
			break;
		case 270:
			x-=2;
			break;
		case 315:
			y-=2;
			x-=2;
			break;
		}
		return new Coords(x,y);
	}
	function moveShot(shot)
	{
		var count=0;
		while(count<=2) {
			count++;
			var newPosition=new Coords(0,0);
			switch(Number(shot.heading))
			{
			case 0:
				if(checkPosition(shot.coords.x,shot.coords.y-1)) {
					shot.heading=180;
					continue;
				}
				else
					newPosition.y=-1;
				break;
			case 45:
				newPosition.x=1;
				newPosition.y=-1;
				if(checkPosition(shot.coords.x+newPosition.x,shot.coords.y+newPosition.y)) {
					shot.heading=getRebound(shot.heading,newPosition,shot.coords.x,shot.coords.y);
					continue;
				} 
				break;
			case 90:
				if(checkPosition(shot.coords.x+1,shot.coords.y)) {
					shot.heading=270;
					continue;
				}
				else
					newPosition.x=1;
				break;
			case 135:
				newPosition.x=1;
				newPosition.y=1;
				if(checkPosition(shot.coords.x+newPosition.x,shot.coords.y+newPosition.y)) {
					shot.heading=getRebound(shot.heading,newPosition,shot.coords.x,shot.coords.y);
					continue;
				} 
				break;
			case 180:
				if(checkPosition(shot.coords.x,shot.coords.y+1)) {
					shot.heading=0;
					continue;
				}
				else
					newPosition.y=1;
				break;
			case 225:
				newPosition.x=-1;
				newPosition.y=1;
				if(checkPosition(shot.coords.x+newPosition.x,shot.coords.y+newPosition.y)) {
					shot.heading=getRebound(shot.heading,newPosition,shot.coords.x,shot.coords.y);
					continue;
				} 
				break;
			case 270:
				if(checkPosition(shot.coords.x-1,shot.coords.y)) {
					shot.heading=90;
					continue;
				}
				else
					newPosition.x=-1;
				break;
			case 315:
				newPosition.x=-1;
				newPosition.y=-1;
				if(checkPosition(shot.coords.x+newPosition.x,shot.coords.y+newPosition.y)) {
					shot.heading=getRebound(shot.heading,newPosition,shot.coords.x,shot.coords.y);
					continue;
				} 
				break;
			}
			break;
		}
		if(count > 2) {
			log("ERROR: infinite loop detected!");
			exit();
		}
		shot.coords.x+=newPosition.x;
		shot.coords.y+=newPosition.y;
		shot.range--;
	}
	function getRebound(h,pos,x,y)
	{
		var finished=false;
		x+=pos.x;
		y+=pos.y;
		
		/* the easy stuff */
		switch(battle.map.data[x-1][y-1].ch) {
		case "\xB3": //vertical bar
			pos.x*=-1;
			finished=true;
			break;
		case "\xC4": //horizontal bar
			pos.y*=-1;
			finished=true;
			break;
		}
		if(!finished) {
			/* the confusing stuff */	
			switch(battle.map.data[x-1][y-1].ch) {
			case "\xD9": //bottom right corner
				if(pos.x > 0 && pos.y > 0) {
					pos.x*=-1;
					pos.y*=-1
					break;
				}
				if(pos.x < 0 && pos.y < 0) {
					if(random(100) >= 50) {
						pos.x*=-1;
					} else {
						pos.y*=-1;
					}
					break;
				}
				if(pos.y > 0) {
					pos.x*=-1;
					break;
				}
				pos.y*=-1;		
				break;
			case "\xDA": //top left corner
				if(pos.x < 0 && pos.y < 0) {
					pos.x*=-1;
					pos.y*=-1
					break;
				}
				if(pos.x > 0 && pos.y > 0) {
					if(random(100) >= 50) {
						pos.x*=-1;
					} else {
						pos.y*=-1;
					}
					break;
				}
				if(pos.y > 0) {
					pos.y*=-1;
					break;
				}
				pos.x*=-1;		
				break;
			case "\xBF": //top right corner
				if(pos.x > 0 && pos.y < 0) {
					pos.x*=-1;
					pos.y*=-1
					break;
				}
				if(pos.x < 0 && pos.y > 0) {
					if(random(100) >= 50) {
						pos.x*=-1;
					} else {
						pos.y*=-1;
					}
					break;
				}
				if(pos.y > 0) {
					pos.y*=-1;
					break;
				}
				pos.x*=-1;		
				break;
			case "\xC0": //bottom left corner
				if(pos.x < 0 && pos.y > 0) {
					pos.x*=-1;
					pos.y*=-1
					break;
				}
				if(pos.x > 0 && pos.y < 0) {
					if(random(100) >= 50) {
						pos.x*=-1;
					} else {
						pos.y*=-1;
					}
					break;
				}
				if(pos.y > 0) {
					pos.x*=-1;
					break;
				}
				pos.y*=-1;		
				break;
			}
		}
		if(pos.x < 0 && pos.y < 0) return 315;
		if(pos.x < 0 && pos.y > 0) return 225;
		if(pos.x > 0 && pos.y > 0) return 135;
		if(pos.x > 0 && pos.y < 0) return 45;
	}
	function killPlayer(player)
	{
		var x=player.coords.x;
		var y=player.coords.y;
		
		console.gotoxy(x,y-2); 
		console.putmsg("\1n\1r\xB0");
		console.down();
		console.left(2);
		console.putmsg("\1n\1r\xB0\xB0\xB0");
		console.down();
		console.left(4);
		console.putmsg("\1n\1r\xB0\xB0\xB0\xB0\xB0");
		console.down();
		console.left(4);
		console.putmsg("\1n\1r\xB0\xB0\xB0");
		console.down();
		console.left(2);
		console.putmsg("\1n\1r\xB0");
		mswait(150);
		console.gotoxy(x-1,y-1); 
		console.putmsg("\1r\1h\xB1\xB1\xB1");
		console.down();
		console.left(3);
		console.putmsg("\1r\1h\xB1\xB1\xB1");
		console.down();
		console.left(3);
		console.putmsg("\1r\1h\xB1\xB1\xB1");
		mswait(150);
		console.gotoxy(x,y-1); 
		console.putmsg("\1y\1h\xB2");
		console.down();
		console.left(2);
		console.putmsg("\1y\1h\xB2\xB2\xB2");
		console.down();
		console.left(2);
		console.putmsg("\1y\1h\xB2");
		mswait(150);
		console.gotoxy(x,y); 
		console.putmsg("\1y\1h\xDB");
		mswait(150);
	}
	function resetPosition()
	{
		unDrawTank(currentPlayer);
		currentPlayer.coords=new Coords(currentPlayer.start.x,currentPlayer.start.y);
		currentPlayer.health=100;
		showPlayerInfo();
		drawTank(currentPlayer);
		drawTurret(currentPlayer);
		send("TANK");
	}
	
	/* map functions */
	function checkPosition(x,y)
	{
		if(!battle.map.data[x-1] || !battle.map.data[x-1][y-1]) return true;
		if(battle.map.data[x-1][y-1].ch == " ") return false;
		return true;
	}
	function checkWalls(position)
	{
		var spots=[];
		
		spots.push(new Coords(position.x-3,position.y-3));
		spots.push(new Coords(position.x-3,position.y-2));
		spots.push(new Coords(position.x-3,position.y-1));
		spots.push(new Coords(position.x-3,position.y));
		spots.push(new Coords(position.x-3,position.y+1));
		
		for(spot=0;spot<spots.length;spot++) {
			for(var x=0;x<5;x++) {
				var spotx=spots[spot].x+x;
				var spoty=spots[spot].y;
				if(battle.map.data[spotx][spoty].ch !== " ") return true;
			}
		}
		return false;
	}
	function checkHit(shot,player)
	{
		var position=player.coords;
		var spots=[];
		spots.push(new Coords(position.x-2,position.y-2));
		spots.push(new Coords(position.x-2,position.y-1));
		spots.push(new Coords(position.x-2,position.y));
		spots.push(new Coords(position.x-2,position.y+1));
		spots.push(new Coords(position.x-2,position.y+2));
		
		for(spot=0;spot<spots.length;spot++) {
			for(var x=0;x<5;x++) {
				var spotx=spots[spot].x+x;
				var spoty=spots[spot].y;
				if(shot.coords.x == spotx && shot.coords.y == spoty) return true;
			}
		}
		return false;
	}

	/* display functions */
	function showPlayerInfo()
	{
		console.home();
		for(var p in battle.players) {
			var player=battle.players[p];
			var name=player.color + players.getAlias(player.name);
			var hline=(player.health<=25?"\1r":player.color) + player.health;
			console.putmsg(" " + name + "\1k\1h [\1n" + player.color + "hp\1h:\1n" + hline + "\1k\1h]");
		}
		console.cleartoeol();
	}
	function drawShot(shot)
	{
		console.gotoxy(shot.coords.x,shot.coords.y);
		console.putmsg(shot.color + "\1h\xF9");
		console.home();
	}
	function unDrawShot(shot)
	{
		console.gotoxy(shot.coords.x,shot.coords.y);
		console.putmsg(" ");
		if(checkHit(shot,battle.players[shot.player])) {
			drawTank(battle.players[shot.player]);
			drawTurret(battle.players[shot.player]);
		}
	}
	function unDrawTank(tank)
	{
		var x=tank.coords.x;
		var y=tank.coords.y;
		switch(Number(tank.heading))
		{
		case 0:
			console.gotoxy(x-2,y-2); 	print("     ");
			console.gotoxy(x-2,y-1); 	print("     ");
			console.gotoxy(x-2,y);   	print("     ");
			console.gotoxy(x-2,y+1); 	print("     ");
			console.gotoxy(x-2,y+2); 	print("     ");
			break;
		case 90:
			console.gotoxy(x-2,y-2); 	print("     ");
			console.gotoxy(x-2,y-1); 	print("     ");
			console.gotoxy(x-2,y);   	print("     ");
			console.gotoxy(x-2,y+1); 	print("     ");
			console.gotoxy(x-2,y+2); 	print("     ");
			break;
		}
	}
	function drawTurret(tank)
	{
		var x=tank.coords.x;
		var y=tank.coords.y;
		var color=tank.color;
		if(tank.health<25) color="\1n\1r";
		else if(tank.health<50) color="\1k\1h";
		
		switch(Number(turretCompass[tank.turret]))
		{
			case 0:
				console.gotoxy(x,y-1);
				console.putmsg("\1h" + color + "\xB3");
				break;
			case 180:
				console.gotoxy(x,y+1);
				console.putmsg("\1h" + color + "\xB3");
				break;
			case 90:
				console.gotoxy(x+1,y);
				console.putmsg("\1h" + color + "\xC4");
				break;
			case 270:
				console.gotoxy(x-1,y);
				console.putmsg("\1h" + color + "\xC4");
				break;
			case 45:
				console.gotoxy(x+1,y-1);
				console.putmsg("\1h" + color + "/");
				break;
			case 135:
				console.gotoxy(x+1,y+1);
				console.putmsg("\1h" + color + "\\");
				break;
			case 225:
				console.gotoxy(x-1,y+1);
				console.putmsg("\1h" + color + "/");
				break;
			case 315:
				console.gotoxy(x-1,y-1);
				console.putmsg("\1h" + color + "\\");
				break;
		}
		console.home();
	}
	function drawTank(tank)
	{
		var x=tank.coords.x;
		var y=tank.coords.y;
		var color=tank.color;
		if(tank.health<25) color="\1n\1r";
		else if(tank.health<50) color="\1k\1h";
		
		switch(Number(tank.heading))
		{
		case 0:
			console.gotoxy(x-2,y-2); 
			console.putmsg("\1k\1h\xFE" + color + "\xDA\xC4\xBF" + "\1k\1h\xFE");
			console.gotoxy(x-2,y-1); 
			console.putmsg("\1k\1h\xFE" + color + "\xB3 \xB3" + "\1k\1h\xFE");
			console.gotoxy(x-2,y); 
			console.putmsg("\1k\1h\xFE" + color + "\xB3O\xB3" + "\1k\1h\xFE");
			console.gotoxy(x-2,y+1); 
			console.putmsg("\1k\1h\xFE" + color + "\xB3 \xB3" + "\1k\1h\xFE");
			console.gotoxy(x-2,y+2); 
			console.putmsg("\1k\1h\xFE" + color + "\xC0\xC4\xD9" + "\1k\1h\xFE");
			break;
		case 90:
			console.gotoxy(x-2,y-2); 
			console.putmsg("\1k\1h\xFE\xFE\xFE\xFE\xFE");
			console.gotoxy(x-2,y-1); 
			console.putmsg("\1h" + color + "\xDA\xC4\xC4\xC4\xBF");
			console.gotoxy(x-2,y); 
			console.putmsg("\1h" + color + "\xB3 O \xB3");
			console.gotoxy(x-2,y+1); 
			console.putmsg("\1h" + color + "\xC0\xC4\xC4\xC4\xD9");
			console.gotoxy(x-2,y+2); 
			console.putmsg("\1k\1h\xFE\xFE\xFE\xFE\xFE");
			break;
		}
	}
	function redraw()
	{
		DrawBattle();
		showPlayerInfo();
		for(ply in players) {
			players[ply].draw();
		}
		console.center("\1k\1hUse Arrow Keys to Move. First player to reach '\1yX\1k' wins. - [\1cQ\1k]uit [\1cR\1k]edraw");
	}
	
	init();
	main();
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
function sendFiles(mask)
{
	stream.sendfile(mask);
}
function storeGame(battle)
{
	//STORE GAME DATA
	var file=new File(battle.dataFile+".tmp");
	file.open("w+");
	
	file.iniSetValue(null,"gameNumber",battle.gameNumber);
	file.iniSetValue(null,"mapFile",battle.mapFile);
	file.iniSetValue(null,"status",battle.status);
	
	if(battle.created) file.iniSetValue(null,"created",battle.created);
	
	for(var p in battle.players) {
		file.iniSetValue("players",p,battle.players[p].position);
	}

	file.close();
	file_remove(battle.dataFile);
	file_rename(file.name,battle.dataFile);
	sendFiles(battle.dataFile);
}

splashStart();
lobby();
splashExit();