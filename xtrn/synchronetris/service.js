var root = argv[0];
js.branch_limit = 0;

load("json-client.js");
load("event-timer.js");
load("funclib.js");
load("sbbsdefs.js");

var game_id = "synchronetris";
var timer = new Timer();
var client = new JSONClient("localhost",10088);

var data = { 
	games:client.read(game_id,"games",1),
	boards:client.read(game_id,"boards",1),
	players:client.read(game_id,"players",1),
	timers:{}
};

var settings = {
	garbage:true,
	min_players:1,
	max_players:3,
	pause:1000,
	pause_reduction:0.7,
	base_points:5,
	lines:10,
	start_delay:5000
};

var status={
	WAITING:-1,
	STARTING:0,
	PLAYING:1,
	FINISHED:2
}

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
				gameNumber = p[0];
				break;
			case "PIECES":
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
	for each(var game in data.games) {
		if(!game.players) {
			deleteGame(game.gameNumber);
			continue;
		}
		var pcount = countMembers(game.players);
		if(pcount == 0) 
			deleteGame(game.gameNumber);
		else if(game.players[update.data.nick]) {
			if(pcount == 1)
				deleteGame(game.gameNumber);
			else 
				deletePlayer(game.gameNumber,update.data.nick);
		}
	}
}

/* delete a game */
function deleteGame(gameNumber) {
	log(LOG_WARNING,"removing empty game #" + gameNumber);
	client.remove(game_id,"games." + gameNumber,2);
	if(data.timers[gameNumber])
		data.timers[gameNumber].abort = true;
	delete data.games[gameNumber];
}

/* delete a player */
function deletePlayer(gameNumber,playerName) {
	log(LOG_WARNING,"removing player from game " + gameNumber);
	client.remove(game_id,"games." + gameNumber + ".players." + playerName,2)
	delete data.games[gameNumber].players[playerName];
}

/* ensure there arent too few or too many players in a game */
function validateGamePlayers(update,gameNumber,playerName) {
	var game = data.games[gameNumber];
	if(!game.players) {
		deleteGame(gameNumber);
		return;
	}
	var pcount = countMembers(game.players);
	if(pcount == 0) 
		deleteGame(gameNumber);
	else if(pcount > settings.max_players)
		deletePlayer(gameNumber,playerName);
	else if(pcount >= settings.min_players) {
		var ready = getReady(game);
		if(ready && game.status == status.WAITING)
			startTimer(game);
		else if(!ready && game.status == status.PLAYING && pcount == 1)
			deletePlayer(gameNumber,playerName);
	}
}

/* check the status of all games (after an update) */
function updateGames() {
	for each(var game in data.games) 
		if(game.status == status.STARTING && !getReady(game)) 
			stopTimer(game);
}

/* stop the countdown */
function stopTimer(game) {
	if(data.timers[game.gameNumber])
		data.timers[game.gameNumber].abort = true;
	game.status = status.WAITING;
	client.write(game_id,"games." + game.gameNumber + ".status",status.WAITING,2);
}

/* start the countdown */
function startTimer(game) {
	data.timers[game.gameNumber] = timer.addEvent(settings.start_delay,false,startGame,[game]);
	game.status = status.STARTING;
	client.write(game_id,"games." + game.gameNumber + ".status",status.STARTING,2);
}

/* check a game for player readiness */
function getReady(game) {
	for each(var p in game.players) {
		if(p.ready == false)
			return false;
	}
	return true;
}

/* generate a game and start */
function startGame(game) {
	/* verify that the game is still ready */
	if(game.status == status.STARTING) {
		log("starting game: " + game.gameNumber);
		client.lock(game_id,"games." + game.gameNumber + ".status",2);
		client.write(game_id,"metadata." + game.gameNumber + ".queue",generatePieces(),2);
		game.status = status.PLAYING;
		client.write(game_id,"games." + game.gameNumber + ".status",status.PLAYING);
		client.unlock(game_id,"games." + game.gameNumber + ".status");
	}
}

/* generate a game */
function generatePieces() {
	var pieces = [];
	for(var p=0;p<10000;p++) 
		pieces.push(getNewPiece());
	return pieces;
}

/* return a new random piece */
function getNewPiece() {
	return random(7)+1;
}

/* initialize service */
function init() {
	js.branch_limit=0;
	client.subscribe(game_id,"games");
	client.subscribe(game_id,"players");
	client.write(game_id,"settings",settings,2);
	client.callback = processUpdate;
	if(!data.games)
		data.games = {};
	if(!data.boards)
		data.boards = {};
	if(!data.players)
		data.players = {};
	log(LOG_INFO,"Synchronetris background service initialized");
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
function Board(gameNumber,players,garbage) {
	this.gameNumber = gameNumber;
	this.players = players;
	this.garbage = garbage;
}

init();
main();