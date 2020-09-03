var root = argv[0];
js.branch_limit = 0;

load("json-client.js");
load("event-timer.js");
load("funclib.js");
load("sbbsdefs.js");
load(root + "mazegen.js");

var server_file = new File(file_cfgname(root, "server.ini"));
server_file.open('r',true);
var serverAddr=server_file.iniGetValue(null,"host","localhost");
var serverPort=server_file.iniGetValue(null,"port",10088);
server_file.close();


var game_id = "mazerace";
var timer = new Timer();
var client = new JSONClient(serverAddr,serverPort);

var data = { 
	games:client.read(game_id,"games",1),
	mazes:client.read(game_id,"mazes",1),
	players:client.read(game_id,"players",1),
	timers:{}
};
var settings = {
	//damage:true,
	damage_qty:5,
	min_players:1,
	max_players:10,
	max_health:100,
	start_delay:5000,
	columns:26,
	rows:11,
	colors:[YELLOW,LIGHTGREEN,LIGHTMAGENTA,LIGHTCYAN,LIGHTRED,WHITE],
	avatars:["\x02","\x9D","\xE8","\xEA","\xEB","\xF9"]
};
var status={
	WAITING:-1,
	STARTING:0,
	RACING:1,
	FINISHED:2
}

/* bitwise compass */
const N=1;
const E=2;
const S=4;
const W=8;

/* process inbound updates */
function processUpdate(update) {
	var playerName = undefined;
	var gameNumber = undefined;
	var raceNumber = undefined;
	
	switch(update.oper.toUpperCase()) {
	case "UNSUBSCRIBE":
		handleDisco(update);
		break;
	case "WRITE":
		var p=update.location.split(".");
		var obj=data;
		while(p.length > 1) {
			var child=p.shift();
			obj=obj[child];
			switch(child.toUpperCase()) {
			case "PLAYERS":
				playerName = p[0];
				break;
			case "GAMES":
			case "MAZES":
				gameNumber = p[0];
				break;
			}
		}
		if(update.data == undefined)
			delete obj[p.shift()];
		else
			obj[p.shift()] = update.data;
		if(playerName != undefined && gameNumber != undefined) 
			validateGamePlayers(update,gameNumber,playerName);
		updateGames();
		break;
	}
}

/* handle a player disconnection */
function handleDisco(update) {
	try {
		for each(var game in data.games) {
			if(!game.players) {
				deleteGame(game.gameNumber);
				continue;
			}
			var pcount = countMembers(game.players);
			if(pcount == 0) {
				deleteGame(game.gameNumber);
			}
			else if(game.players[update.data.nick]) {
				if(pcount == 1)
					deleteGame(game.gameNumber);
				else 
					deletePlayer(game.gameNumber,update.data.nick);
			}
		}
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* delete a game */
function deleteGame(gameNumber) {
	try {
		log(LOG_DEBUG,"removing empty game #" + gameNumber);
		client.remove(game_id,"games." + gameNumber,2);
		if(data.timers[gameNumber]) {
			data.timers[gameNumber].abort = true;
		}
		delete data.games[gameNumber];
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* delete a player */
function deletePlayer(gameNumber,playerName) {
	try {
		log(LOG_DEBUG,"removing player from game " + gameNumber);
		client.remove(game_id,"games." + gameNumber + ".players." + playerName,2)
		delete data.games[gameNumber].players[playerName];
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* ensure there arent too few or too many players in a game */
function validateGamePlayers(update,gameNumber,playerName) {
	try {
		var game = data.games[gameNumber];
		if(!game.players) {
			deleteGame(gameNumber);
			return;
		}
		var pcount = countMembers(game.players);
		if(pcount == 0) {
			deleteGame(gameNumber);
		}
		else if(pcount > settings.max_players) {
			deletePlayer(gameNumber,playerName);
		}
		else if(pcount >= settings.min_players) {
			var ready = getReady(game);
			if(ready && game.status == status.WAITING)
				startTimer(game);
			else if(!ready && game.status == status.RACING && pcount == 1)
				deletePlayer(gameNumber,playerName);
		}
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* check the status of all games (after an update) */
function updateGames() {
	try {
		for each(var game in data.games) {
			if(game.status == status.STARTING && !getReady(game)) {
				stopTimer(game);
			}
		}
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* stop the countdown */
function stopTimer(game) {
	try {
		if(data.timers[game.gameNumber]) {
			data.timers[game.gameNumber].abort = true;
		}
		game.status = status.WAITING;
		client.write("mazerace","games." + game.gameNumber + ".status",status.WAITING,2);
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* start the countdown */
function startTimer(game) {
	try {
		data.timers[game.gameNumber] = timer.addEvent(settings.start_delay,false,startGame,[game]);
		game.status = status.STARTING;
		client.write("mazerace","games." + game.gameNumber + ".status",status.STARTING,2);
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* check a game for player readiness */
function getReady(game) {
	try {
		for each(var p in game.players) {
			if(p.ready == false) {
				return false;
			}
		}
		return true;
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* generate a maze and start the race */
function startGame(game) {
	try {
		/* verify that the game is still ready */
		if(game.status == status.STARTING) {
			log(LOG_DEBUG,"starting race: " + game.gameNumber);
			client.lock("mazerace","games." + game.gameNumber + ".status",2);
			game.status = status.RACING;
			createMaze(game);
			client.write("mazerace","games." + game.gameNumber + ".status",status.RACING);
			client.unlock("mazerace","games." + game.gameNumber + ".status");
		}
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* generate a maze */
function createMaze(game) {
	try {
		client.lock("mazerace","mazes." + game.gameNumber,2);
		
		var maze = generateMaze(settings.columns,settings.rows);
		var start = getRandomCorner(maze,0,E);
		var finish = getRandomCorner(maze,settings.columns-1,W);
		var race = new Race(game.gameNumber,game.players,game.damage,game.fog,maze,start,finish);
		
		data.mazes[game.gameNumber] = race;
		client.write("mazerace","mazes." + game.gameNumber,race);
		client.unlock("mazerace","mazes." + game.gameNumber);
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* locate start and finish points */
function getRandomCorner(maze,col,side)	{
	try {
		var count = 0;
		var row = random(settings.rows);
		while(count < settings.rows) {
			if((maze[row][col]&side) && (maze[row][col]&S || maze[row][col]&N)) 
				break;
			count++;
			row++;
			if(row == settings.rows)
				row = 0;
		}
		return {
			x:col,
			y:row
		};
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* initialize service */
function init() {
	try {
		js.branch_limit=0;
		client.subscribe(game_id,"games");
		client.subscribe(game_id,"players");
		client.write(game_id,"settings",settings,2);
		client.callback = processUpdate;
		if(!data.games)
			data.games = {};
		if(!data.mazes)
			data.mazes = {};
		if(!data.players)
			data.players = {};
		log(LOG_INFO,"Maze Race background service initialized");
	}
	catch(e) {
		log(LOG_ERROR,e);
	}
}

/* main loop */
function main() {
	while(!js.terminated && !parent_queue.poll()) {
		if(client.socket.poll(.1))
			client.cycle();
		timer.cycle();
	}
}

/* race object */
function Race(gameNumber,players,damage,fog,maze,start,finish) {
	this.gameNumber = gameNumber;
	this.players = players;
	this.damage = damage;
	this.fog = fog;
	this.maze = maze;
	this.start = start;
	this.finish = finish;
}

init();
main();