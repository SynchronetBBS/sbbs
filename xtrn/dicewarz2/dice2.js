load("sbbsdefs.js");
load("graphic.js");
load("chateng.js");
load("funclib.js");

var game_dir=js.exec_dir;
	
load(game_dir+"maps.js");
load(game_dir+"menu.js");

var oldpass=console.ctrlkey_passthru;
console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
bbs.sys_status|=SS_MOFF;

var menu;
var chat=		new ChatEngine(game_dir);
var players=	new PlayerData(game_dir+settings.player_file);
var game_background=loadGraphic(game_dir+settings.background_file);
var lobby_background=loadGraphic(game_dir+settings.lobby_file);

//GLOBAL GAME FUNCTIONS
function splashStart()
{
	console.clear();
	var splash_filename=game_dir + "welcome.bin";
	if(file_exists(splash_filename)) {
		var splash=new Graphic(80,24);
		splash.load(splash_filename);
		splash.draw();
		
		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		while(console.inkey(K_NOECHO|K_NOSPIN)=="");
	}
}
function splashExit()
{
	stream.close();
	chat.exit();
	console.ctrlkey_passthru=oldpass;
	bbs.sys_status&=~SS_MOFF;
	console.clear(ANSI_NORMAL);
	var splash_filename=game_dir + "exit.bin";
	if(file_exists(splash_filename)) {
		var splash_size=file_size(splash_filename);
		splash_size/=2;		
		splash_size/=80;	
		var splash=new Graphic(80,splash_size);
		splash.load(splash_filename);
		splash.draw();
		
		console.gotoxy(1,23);
		console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
		console.getkey(K_NOSPIN|K_NOCRLF);
	}
	exit();
}
function loadGraphic(filename)
{
	log("loading graphic: " + filename);
	var graphic=new Graphic(80,24);
	graphic.load(filename);
	return graphic;
}
function chatInput()
{
	chat.input_line.clear();
	while(1) {
		var key=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
		if(key) {
			chat.processKey(key);
			switch(key) {
				case '\r':
				case '\n':
					hideChatLine();
					return true;
				default:
					break;
			}
		}
		cycle();
	}
}
function hideChatLine()
{
	console.gotoxy(chat.input_line);
	console.cleartoeol(BG_BROWN);
}
function menuPrompt(string,append)
{
	if(append) console.popxy();
	else {
		menu.clear();
		console.gotoxy(menu.x,menu.y);
	}
	console.attributes=BG_BROWN+BLACK;
	console.putmsg(string,P_SAVEATR);
	console.pushxy();
	return console.getkey(K_NOECHO|K_NOCRLF|K_UPPER|K_NOSPIN);
}
function menuText(string,append)
{
	if(append) console.popxy();
	else {
		menu.clear();
		console.gotoxy(menu.x,menu.y);
	}
	console.attributes=BG_BROWN+BLACK;
	console.putmsg(string,P_SAVEATR);
	console.pushxy();
}
function viewInstructions(section)
{

}
function cycle()
{
	chat.receive();
}

//LOBBY LOOP
function lobby()
{
	initMenu();
	initChat();
	
	function main()
	{
		redraw();
		while(1)
		{
			var cmd=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
			if(cmd) {
				menu.clear();
				switch(cmd)
				{
				case "R":
					viewRankings();
					break;
				case "S":
					if(selectGame()) return true;
					break;
				case "I":
					viewInstructions();
					break;
				case "B":
					createNewGame();
					break;
				case "C":
					chatInput();
					break;
				case KEY_UP:
				case KEY_DOWN:
				case KEY_HOME:
				case KEY_END:
					chat.processKey(cmd);
					break;
				case "\x1b":
				case "Q":
					return false;
				default:
					break;
				}
				menu.draw();
			}
			lobbyCycle();
		}
	}
	function lobbyCycle()
	{
		game_data.update();
		cycle();
	}
	function wrap(msg,lst)
	{
		console.pushxy();
		console.putmsg(msg);
		console.putmsg(" \1w\1h: ");
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
			console.putmsg("\1h\1g" + item);
			col += console.strlen(item);
		}
		console.crlf();
		console.right();
	}
	function gameList()
	{
		var sorted=sortGameData(game_data.list);
		clearBlock(2,2,78,16);
		console.gotoxy(2,2);
		wrap("\1gGames in progress          ",sorted.started);
		wrap("\1gGames needing more players ",sorted.waiting);
		wrap("\1gCompleted games            ",sorted.finished);
		wrap("\1gYou are involved in games  ",sorted.yourgames);
		wrap("\1gIt is your turn in games   ",sorted.yourturn);
		wrap("\1gYou are eliminated in games",sorted.eliminated);
		wrap("\1gYou have won games         ",sorted.yourwins);
		wrap("\1gSingle-player games        ",sorted.singleplayer);
	}
	function initChat()
	{
		chat.init("dice warz",78,5,2,19);
		chat.input_line.init(10,18,70,"\0017","\1k");
	}
	function initMenu()
	{
		var menu_items=[	"",
							"\1w\1h~R\1n\1k\0013ankings", 
							"\1w\1h~S\1n\1k\0013elect game",
							"\1w\1h~B\1n\1k\0013egin new game",
							"\1w\1h~H\1n\1k\0013elp",
							"\1w\1h~C\1n\1k\0013hat",
							"\1w\1h~Q\1n\1k\0013uit"];
		menu=new Menu(menu_items,BG_BROWN,10,24);	
	}
	function selectGame()
	{
		if(game_data.count()==0) {
			menuPrompt("No games to select \1w\1h[press any key]");
			return false;
		}
		while(1) {
			menuText("Game number: \1w\1h");
			var num=console.getstr(game_data.last_game_number.toString().length,K_NUMBER|K_NOCRLF|K_NOSPIN);
			if(!num) {
				return false;
			}
			if(game_data.list[num]) {
				run(game_data.list[num]);
				return true;
			} else {
				menuText("\1w\1hNo such game!",true);
				menuPrompt(" [press any key]",true);
			}
		}
	}
	function startGame(map)
	{
		if(map.players.length==1) map.single_player=true;
		addComputers(map);
		generateMap(map);
		shufflePlayers(map);
		dispersePlayers(map);
		map.in_progress=true;
		game_data.save(map);
	}
	function joinGame(map)
	{
		addPlayer(map,user.alias,getVote());
	}
	function createNewGame()
	{
		response=menuPrompt("Begin a new game? \1w\1h[Y/n]: ");
		if(response!=='Y') return false;
		var new_game=new MapData();

		while(1) {
			response=menuPrompt("Single player game? \1w\1h[y/N]: ");
			if(response=='\x1b' || response=='Q') {
				return false;
			} else if(response=='Y') {
				addPlayer(new_game,user.alias,true);
				startGame(new_game);
				run(new_game);
				break;
			} else if(response=='N') {
				game_data.save(new_game);
				break;
			} else {
				menuPrompt("Invalid response \1w\1h[press any key]",true);
			}
		}
	}
	function getVote()
	{
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		lobby_background.draw();
		menu.draw();
		chat.redraw();
		hideChatLine();
		gameList();
	}
	
	main();
}
//GAME LOOP
function run(map)
{
	var activity_window=new Graphic(32,5);
	var ax=48;
	var ay=12;
	var player=findPlayer(map,user.alias);
	var update=true;
	
	function main()
	{
		menu.draw();
		turnAlert();
		var coords=false;
		if(map.single_player && player>=0 && map.players[map.turn].AI) {
			load(true,game_dir + "ai.js",map.game_number,game_dir);
		}
		
		while(1)
		{
			gameCycle();
			var cmd=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
			if(cmd && menu.items[cmd] && menu.items[cmd].enabled) {
				menu.clear();
				switch(cmd)
				{
				case "I":
					viewInstructions();
					break;
				case "T":
					takeTurn();
					break;
				case "A":
					coords=attack(coords);
					listPlayers();
					break;
				case "E":
					endturn();
					break;
				case "F":
					forfeit();
					break;
				case "C":
					chatInput();
					break;
				case "R":
					redraw();
					break;
				case KEY_UP:
				case KEY_DOWN:
				case KEY_HOME:
				case KEY_END:
					chat.processKey(cmd);
					break;
				case "\x1b":
				case "Q":
					return lobby();
				default:
					break;
				}
				setMenuCommands();
			}
		}
	}
	function endturn()
	{
		reinforce();
		nextTurn(map);
		map.attacking=map.turn;
		game_data.saveData(map);
		if(map.players[map.turn].AI) {
			load(true,game_dir + "ai.js",map.game_number,game_dir);
		}
		listPlayers();
	}
	function reinforce()
	{
		var player_tiles=getPlayerTiles(map,map.turn);
		var reinforcements=countConnected(map,player_tiles,map.turn);
		var placed=placeReinforcements(map,player_tiles,reinforcements);
		if(placed.length>0) {
			for(var p=0;p<placed.length;p++) {
				var home=map.tiles[placed[p]].home;
				drawSector(map,home.x,home.y);
			}
			activityAlert("\1n\1y" + map.players[map.turn].name + " placed " + placed.length + " dice");
		}
		var reserved=placeReserves(map,reinforcements-placed.length);
		if(reserved>0) {
			activityAlert("\1n\1y" + map.players[map.turn].name + " reserved " + reserved + " dice");
		}
	}
	function turnAlert()
	{
		if(map.turn==player) activityAlert("\1c\1hIt is your turn");
		else activityAlert("\1n\1cIt is " + map.players[map.turn].name + "'s turn");
	}
	function takeTurn()
	{
		map.attacking=player;
	}
	function attack(coords)
	{
		if(!coords) coords=new Coords(map.width/2,map.height/2,0);
		var attacking;
		var attacker=map.players[player].name;
		var defending;
		var defender;

		activityAlert("\1nChoose \1r\1hattacking \1nterritory");
		while(1) {
			coords=select(coords);
			if(!coords) return false;
			attacking=map.tiles[map.grid[coords.x][coords.y]];
			if(attacking.owner==player) {
				if(attacking.dice>1) break;
				else activityAlert("\1r\1hNot enough dice to attack");
			} else {
				activityAlert("\1r\1hNot your territory");
			}
		}
		activityAlert("\1nChoose \1r\1htarget \1nterritory");
		while(1) {
			coords=select(coords);
			if(!coords) return false;
			defending=map.tiles[map.grid[coords.x][coords.y]];
			if(defending.owner!=player) {
				if(connected(attacking,defending,map)) {
					defender=map.players[defending.owner].name;
					break;
				} else {
					activityAlert("\1r\1hNot connected");
				}
			} else {
				activityAlert("\1r\1hNot an enemy territory");
			}
		}
		
		chat.chat_room.clear();
		var a=new Roll(attacking.owner);
		for(var r=0;r<attacking.dice;r++) {
			var roll=random(6)+1;
			a.roll(roll);
		}
		showRoll(a.rolls,LIGHTGRAY+BG_RED,chat.chat_room.x,chat.chat_room.y);
		var d=new Roll(defending.owner);
		for(var r=0;r<defending.dice;r++) {
			var roll=random(6)+1;
			d.roll(roll);
		}
		showRoll(d.rolls,BLACK+BG_LIGHTGRAY,chat.chat_room.x,chat.chat_room.y+3);
		chat.chat_room.draw();
		battle(a,d);
		var data=new Data("battle");
		data.a=a;
		data.d=d;
		stream.send(data);
		
		if(a.total>d.total) {
			defending.assign(attacking.owner,attacking.dice-1);
			if(countTiles(map,defending.owner)==0) map.players[defending.owner].active=false;
			drawTile(map,defending);
		} 
		attacking.dice=1;
		drawSector(map,attacking.home.x,attacking.home.y);
		
		game_data.saveActivity(map,attacker + " attacked " + defender + ": " + attacking.dice + " vs " + defending.dice);
		game_data.saveActivity(map,attacker + ": " + a.total + " " + defender + ": " + d.total);
		game_data.saveTile(map,attacking);
		game_data.saveTile(map,defending);
		
		updateStatus(map);
		
		return coords;
	}
	function battle(a,d) 
	{
		activityAlert("\1n" + map.players[a.pnum].name + " vs. " + map.players[d.pnum].name);
		activityAlert("\1n" + map.players[a.pnum].name + " rolls " + a.rolls.length + (a.rolls.length>1?" dice":" die")+"\1h: \1c" + a.total);
		activityAlert("\1n" + map.players[d.pnum].name + " rolls " + d.rolls.length + (d.rolls.length>1?" dice":" die")+"\1h: \1c" + d.total);
	}
	function select(start)
	{
		var posx=start.x;
		var posy=start.y;
		var yoffset=start.z;
		var cursor="\0017\1k\xC5";
		getxy(posx,posy);
		if(yoffset>0) console.down();
		console.pushxy();
		console.putmsg(cursor,P_SAVEATR);
		
		while(1)
		{
			var xoffset=posx%2;
			var cmd=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
			if(cmd) {
				drawSector(map,posx,posy);
				console.popxy();
				switch(cmd)
				{
				case "R":
					redraw();
					console.popxy();
					break;
				case "C":
					chatInput();
					console.popxy();
					break;
				case KEY_UP:
					if(posy<=0) break;
					if(yoffset==1) yoffset--;
					else {
						yoffset++;
						posy--;
					}
					console.up();
					break;
				case KEY_DOWN:
					if(posy==map.height-1 && yoffset==1) break;
					if(posy>=map.height) break;
					if(yoffset==0) yoffset++;
					else {
						yoffset--;
						posy++;
					}
					console.down();
					break;
				case KEY_LEFT:
					if(posx==0) break;
					if(yoffset==0) {
						yoffset++;
						if(xoffset==0) posy--;
					} else {
						yoffset--;
						if(xoffset==1) posy++;
					}
					posx--;
					console.left(2);
					break;
				case KEY_RIGHT:
					if(posx==map.width-1) break;
					if(yoffset==0) {
						yoffset++;
						if(xoffset==0) posy--;
					} else {
						yoffset--;
						if(xoffset==1) posy++;
					}
					posx++;
					console.right(2);
					break;
				case KEY_HOME:
					if(posx==0) posx=map.width-1;
					else posx=0;
					posy=0;
					getxy(posx,posy);
					console.down();
					break;
				case KEY_END:
					if(posx==0) posx=map.width-1;
					else posx=0;
					posy=map.height-1;
					getxy(posx,posy);
					console.down();
					break;
				case "\r":
				case "\n":
					var tile=map.grid[posx][posy];
					var north=map.grid[posx][posy-1];
					var valid=true;
					if(yoffset==1) {
						if(tile==undefined) valid=false;
					} else if(tile==undefined && north==undefined) valid=false;
					else if(tile!==north) {
						if(north>=0 && tile==undefined) posy-=1;
						else if(tile>=0 && north>=0) valid=false;
					}
					if(valid) {
						var position=new Coords(posx,posy,yoffset);
						return position;
					}
					activityAlert("\1r\1hInvalid selection");
					console.popxy();
					break;
				case "\x1b":
				case "Q":
					return false;
				default:
					break;
				}
				console.pushxy();
				console.putmsg(cursor,P_SAVEATR);
				gameCycle();
			}
		}
		
	}
	function initMenu()
	{
		var menu_items=[	"",
							"\1w\1h~T\1n\1k\0013ake turn",
							"\1w\1h~A\1n\1k\0013ttack", 
							"\1w\1h~E\1n\1k\0013nd turn",
							"\1w\1h~R\1n\1k\0013edraw",
							"\1w\1h~H\1n\1k\0013elp",
							"\1w\1h~F\1n\1k\0013orfeit",
							"\1w\1h~C\1n\1k\0013hat",
							"\1w\1h~Q\1n\1k\0013uit"];
		menu=new Menu(menu_items,BG_BROWN,10,24);	
		setMenuCommands();
	}
	function setMenuCommands()
	{
		update=true;
		if(player>=0 && map.in_progress) {
			if(player==map.attacking) {
				menu.enable("A");
				menu.enable("E");
				menu.enable("F");
				menu.disable("T");
				return;
			} 
			if(player==map.turn) {
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
	function initChat()
	{
		chat.init("dice warz game #" + map.game_number,31,6,48,18); 
		chat.input_line.init(10,24,70,"\0017","\1k");
	}
	function initGame()
	{
	}
	function activityAlert(msg)
	{
		activity_window.putmsg(undefined,undefined,msg+"\r\n",undefined,true);
		activity_window.draw(ax,ay);
	}
	function gameCycle()
	{
		cycle();
		var data=stream.receive();
		if(data) {
			debug(data,LOG_WARNING);
			switch(data.type)
			{
				case "battle":
					battle(data.a,data.d);
					break;
				case "turn":
					map.turn=data.turn;
					turnAlert();
					setMenuCommands();
					break;
				case "tile":
					map.tiles[data.tile.id].assign(data.tile.owner,data.tile.dice);
					drawTile(map,map.tiles[data.tile.id]);
					break;
				case "activity":
					activityAlert(data.activity);
					break;
				default:
					break;
			}
			listPlayers();
		}
		if(update) {
			menu.draw();
			update=false;
		}
	}
	function listPlayers()
	{
		var x=48;
		var y=3;
		for(var p=0;p<map.players.length;p++) {
			var plyr=map.players[p];
			var fg=plyr.active?getColor(settings.foreground_colors[p]):BLACK;
			var bg=plyr.active?getColor(settings.background_colors[p]):BG_BLACK;
			var txt=plyr.active?getColor(settings.text_colors[p]):DARKGRAY;
			
			console.gotoxy(x,y+p);
			console.attributes=BG_BLACK + fg;
			console.putmsg("\xDE",P_SAVEATR);
			console.attributes=bg + txt;
			console.putmsg(printPadded(plyr.name,16),P_SAVEATR);
			console.attributes=BG_BLACK + fg;
			console.putmsg("\xDD",P_SAVEATR);
			console.right();
			console.attributes=bg + txt;
			console.putmsg(printPadded(countTiles(map,p),4," ","right"),P_SAVEATR);
			console.attributes=bg + BLACK;
			console.putmsg("\xB3",P_SAVEATR);
			console.attributes=bg + txt;
			console.putmsg(printPadded(countDice(map,p),3," ","right"),P_SAVEATR);
			console.attributes=bg + BLACK;
			console.putmsg("\xB3",P_SAVEATR);
			console.attributes=bg + txt;
			console.putmsg(printPadded(plyr.reserve+" ",4," ","right"),P_SAVEATR);
		}
	}
	function redraw()
	{
		console.clear(ANSI_NORMAL);
		game_background.draw();
		drawMap(map);
		listPlayers();
		chat.redraw();
		activity_window.draw(48,12);
		hideChatLine();
	}
	
	initMenu();
	initChat();
	initGame();
	redraw();
	main();
}

splashStart();
while(1) if(!lobby()) break;
splashExit();
