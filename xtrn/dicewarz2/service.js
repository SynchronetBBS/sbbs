var root = argv[0];
if(!file_exists(root + "server.ini")) {
	throw("server initialization file missing");
}
/* load server connection information from server.ini */
var server_file = new File(root + "server.ini");
server_file.open('r',true);
var serverAddr=server_file.iniGetValue(null,"host");
var serverPort=server_file.iniGetValue(null,"port");
server_file.close();

load("json-client.js");
load("event-timer.js");
load("funclib.js");
load("sbbsdefs.js");

var game_id = "dicewarz2";
var client = new JSONClient(serverAddr,serverPort);
var settings = loadSettings("dice.ini");
var aiTakingTurns = {};
//var aiSettings = loadAI("ai.ini");

var data = { 
	games:client.read(game_id,"games",1),
	players:client.read(game_id,"players",1)
};
var status = {
	NEW:-1,
	PLAYING:0,
	FINISHED:1
}

/* load game settings */
function loadSettings(filename) {
	var file=new File(root + filename);
	file.open('r',true);
	
	var data=file.iniGetObject(null);
	data.background_colors=file.iniGetObject("background_colors");
	data.foreground_colors=file.iniGetObject("foreground_colors");
	data.text_colors=file.iniGetObject("text_colors");
	data.point_set=file.iniGetObject("point_set");
	
	file.close();
	return data;
}

/* check initial status of all games (may have activity after crash or reboot) */
function updateGames() {
	for each(var g in data.games) {
		updateStatus(g.gameNumber);
	}
}

/* process inbound updates */
function processUpdate(update) {
	var playerName = undefined;
	var gameNumber = undefined;
	
	/* iterate location and parse variables */
	var p=update.location.split(".");
	var obj=data;
	while(p.length > 1) {
		var child=p.shift();
		if(obj)
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
	
	switch(update.oper.toUpperCase()) {
	case "SUBSCRIBE":
		if(gameNumber) {
			updateStatus(gameNumber);
		}
		break;
	case "UNSUBSCRIBE":
		if(gameNumber) {
			updateStatus(gameNumber);
		}
		else {
			updateGames();
		}
		break;
	case "WRITE":
		/* update local data to match what was received */
		var child = p.shift();
		if(obj) {
			if(update.data == undefined)
				delete obj[child];
			else
				obj[child] = update.data;
		}	
		if(gameNumber) {
			updateStatus(gameNumber);
		}
		break;
	}
}

/* monitor game status? */
function updateStatus(gameNumber) {
	var game = loadGame(gameNumber);
	if(!game)
		return false;
	
	if(game.status == status.PLAYING) {
		updateTurn(game);
	}
	else if(game.status == status.NEW) {
		var pcount=0;
		for(var p in game.players) {
			if(game.players[p])
				pcount++;
		}
		if(pcount == 0) {
			deleteGame(gameNumber);
		}
	}
	else if(game.status == status.FINISHED) {
		var online = client.who(game_id,"maps." + gameNumber);
		if(online.length == 0)
			deleteGame(gameNumber);
	}
}

/* handle turn update, possibly launch AI background thread */
function updateTurn(game) {
	
	if(game.players[game.turn].AI) {
		/* if we have already launched the background thread, abort */
		if(aiTakingTurns[game.gameNumber]) 
			return;
		/* disable this function until it is a human player's turn 
		(to avoid launching multiple background threads) */
		aiTakingTurns[game.gameNumber] = true;
		
		/* launch ai player background client */
		load(true,root + "ai.js",game.gameNumber,root);
	}
	else {
		/* reset this function if we have reached a human player */
		aiTakingTurns[game.gameNumber] = false;
	}
}

/* delete a game */
function deleteGame(gameNumber) {
	log(LOG_WARNING,"removing game #" + gameNumber);
	client.remove(game_id,"games." + gameNumber,2);
	client.remove(game_id,"maps." + gameNumber,2);
	client.remove(game_id,"metadata." + gameNumber,2);
	delete data.games[gameNumber];
}

/* load game data from database */
function loadGame(gameNumber) {
	data.games[gameNumber] = client.read(game_id,"games." + gameNumber,1);
	return data.games[gameNumber];
}

/* delete a player */
function deletePlayer(gameNumber,playerName) {
	log(LOG_WARNING,"removing player from game " + gameNumber);
	client.remove(game_id,"games." + gameNumber + ".players." + playerName,2)
	delete data.games[gameNumber].players[playerName];
}

/* initialize service */
function open() {
	js.branch_limit=0;
	client.subscribe(game_id,"games");
	client.subscribe(game_id,"players");
	client.write(game_id,"settings",settings,2);
	client.callback = processUpdate;
	if(!data.games)
		data.games = {};
	if(!data.players)
		data.players = {};
	
	updateGames();
	log(LOG_INFO,"Dicewarz II background service initialized");
}

function close() {
	client.unsubscribe(game_id,"games");
	client.unsubscribe(game_id,"players");
	log("terminating dicewarz2 background service");
}

/* main loop */
function main() {
	while(!js.terminated && !parent_queue.poll()) {
		if(client.socket.poll(.1))
			client.cycle();
	}
}

open();
main();
close();