var root = argv[0];

load("json-client.js");
load("event-timer.js");
load("funclib.js");
load("sbbsdefs.js");

var game_id = "synchronetris";
var timer = new Timer();
var client = new JSONClient("localhost",10088);

var data = { 
	games:client.read(game_id,"games",1),
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
	SYNCING:1,
	PLAYING:2,
	FINISHED:3
}

/* process inbound updates */
function processUpdate(update) {
	var playerName = undefined;
	var gameNumber = undefined;
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
		case "METADATA":
			gameNumber = p[0];
			break;
		}
	}
	
	switch(update.oper.toUpperCase()) {
	case "SUBSCRIBE":
		if(gameNumber)
			updatePlayers(gameNumber,update.data);
		break;
	case "UNSUBSCRIBE":
		handleDisco(update);
		break;
	case "WRITE":
		var child = p.shift();
		if(update.data == undefined)
			delete obj[child];
		else
			obj[child] = update.data;
		if(child.toUpperCase() == "STATUS")
			updateStatus(update,gameNumber);
		else if(playerName !== undefined && gameNumber !== undefined) 
			updateGame(update,gameNumber,playerName);
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
function updateGame(update,gameNumber,playerName) {
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
		if(ready && (game.status == status.WAITING || game.status == status.FINISHED))
			startTimer(game);
		else if(!ready && game.status == status.STARTING) 
			stopTimer(game);
	}
}

/* handle game status update */
function updateStatus(update,gameNumber) {
	var game = data.games[gameNumber];
	switch(update.data) {
	case status.FINISHED:
		resetPlayers(game);
		break;
	}
}

/* monitor player subscriptions to games */
function updatePlayers(gameNumber,subscription) {
	var game = data.games[gameNumber];
	game.players[subscription.nick].synced = true;
	for each(var p in game.players) {
		if(!p.synced)
			return;
	}
	startGame(game);
}

/* reset all players to "not ready" */
function resetPlayers(game) {
	for each(var p in game.players) {
		p.ready = false;
		client.write(game_id,"games." + game.gameNumber + ".players." + 
			p.name + ".ready",p.ready,2);
	}
}

/* stop the countdown */
function stopTimer(game) {
	if(data.timers[game.gameNumber])
		data.timers[game.gameNumber].abort = true;
	game.status = status.WAITING;
	client.write(game_id,"games." + game.gameNumber + ".status",game.status,2);
}

/* start the countdown */
function startTimer(game) {
	data.timers[game.gameNumber] = timer.addEvent(settings.start_delay,false,startSync,[game]);
	game.status = status.STARTING;
	client.write(game_id,"games." + game.gameNumber + ".status",game.status,2);
}

/* check a game for player readiness */
function getReady(game) {
	for each(var p in game.players) {
		if(p.ready == false)
			return false;
	}
	return true;
}

/* flag game for player sync */
function startSync(game) {
	/* verify that the game is still ready */
	if(game.status == status.STARTING) {
		game.status = status.SYNCING;
		client.subscribe(game_id,"metadata." + game.gameNumber);
		client.write(game_id,"metadata." + game.gameNumber + ".queue",generatePieces(),2);
		client.write(game_id,"games." + game.gameNumber + ".status",game.status,2);
	}
}

/* generate a game and start */
function startGame(game) {
	game.status = status.PLAYING;
	client.unsubscribe(game_id,"metadata." + game.gameNumber);
	client.lock(game_id,"games." + game.gameNumber + ".status",2);
	client.write(game_id,"games." + game.gameNumber + ".status",game.status);
	client.unlock(game_id,"games." + game.gameNumber + ".status");
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

init();
main();