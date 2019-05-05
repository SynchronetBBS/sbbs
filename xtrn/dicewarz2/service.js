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

load("backgroundlog.js");
load("json-client.js");
load("event-timer.js");
load("funclib.js");
load("sbbsdefs.js");
load(root + "diceobj.js");

var game_id = "dicewarz2";
var timer = new Timer();
var client = new JSONClient(serverAddr,serverPort);
var settings = loadSettings("dice.ini");
var ai = loadAI("ai.ini");
var aiTakingTurns = {};

var data = { 
	games:client.read(game_id,"games",1),
	players:client.read(game_id,"players",1)
};

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

/* load AI settings */
function loadAI(filename) {
	var file=new File(root + filename);
	file.open('r',true);
	
	var data = {};
	var names = file.iniGetSections();
	for(var n=0;n<names.length;n++) {
		var name = names[n];
		data[name] = file.iniGetObject(name);
	}
	file.close();
	return data;
}

/* check initial status of all games (may have activity after crash or reboot) */
function updateGames() {
	for each(var g in data.games) {
		try {
			updateStatus(g.gameNumber);
		}
		catch(e) {
			log(LOG_ERROR,"error updating game");
			var gstring = "games: ";
			for(var g in data.games) {
				gstring += g + " ";
			}
			log(LOG_ERROR,gstring);
		}
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
		log(LOG_DEBUG,"updating turn info for game: " + game.gameNumber);
		updateTurn(game);
	}
	else if(game.status == status.NEW) {
		aiTakingTurns[gameNumber] = false;
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
		aiTakingTurns[gameNumber] = false;
		var online = client.who(game_id,"maps." + gameNumber);
		if(online.length == 0)
			deleteGame(gameNumber);
	}
}

/* handle turn update, possibly launch AI background thread */
function updateTurn(game) {
	// log(LOG_WARNING,
		// "game: " + game.gameNumber + 
		// " turn: " + game.turn + 
		// " name: " + game.players[game.turn].name + 
		// " ai: " + game.players[game.turn].AI);
	if(game.players[game.turn].AI) {
		/* if we have already launched the background thread, abort */
		if(aiTakingTurns[game.gameNumber]) {
			log(LOG_DEBUG,"ai already loaded.. ignoring turn update");
			return;
		}
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
	log(LOG_DEBUG,"removing game #" + gameNumber);
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
	log(LOG_DEBUG,"removing player from game " + gameNumber);
	client.remove(game_id,"games." + gameNumber + ".players." + playerName,2)
	delete data.games[gameNumber].players[playerName];
}

/* scan games for inactivity */
function scanInactivity() {
	var timeout = settings.inactivity_timeout * 24 * 60 * 60 * 1000;
	for(var g in data.games) {
		var game = data.games[g];
		try {
			log(LOG_DEBUG,"game " + game.gameNumber + " last turn: " + game.last_turn + " inactive: " + (Date.now() - game.last_turn));
			if(Date.now() - game.last_turn >= timeout) {
				var player = game.players[game.turn];
				if(game.single_player) {
					scoreForfeit(player);
					deleteGame(game.gameNumber);
					log(LOG_DEBUG,"inactive game deleted");
				}
				else if(game.status == status.PLAYING) {
					player.name += " AI";
					player.AI = {
						sort:"random",
						check:"random",
						qty:"single",
						turns:0,
						moves:0
					}
					client.write(game_id,"games." + game.gameNumber + ".players." + game.turn,player,2)
					updateTurn(game);
					log(LOG_DEBUG,"inactive player changed to AI");
				}
			}
		}
		catch(e) {
			log(LOG_ERROR,"error scanning game for inactivity: " + g);
			deleteGame(g);
		}

	}
}

/* forfeit points */
function scoreForfeit(player) {
		client.lock(game_id,"scores." + player.name,2);
		
		var score=client.read(game_id,"scores." + player.name);
		if(!score)
			score=new Score(player.name,player.system);
			
		score.losses++;
		score.points=Number(score.points) + Number(settings.point_set.forfeit);
		if(score.points < settings.min_points)
			score.points = Number(settings.min_points);
		
		client.write(game_id,"scores." + player.name,score);
		client.unlock(game_id,"scores." + player.name);
}

/* initialize service */
function open() {
	js.branch_limit=0;
	client.subscribe(game_id,"games");
	client.subscribe(game_id,"players");
	client.write(game_id,"settings",settings,2);
	client.write(game_id,"ai",ai,2);
	client.callback = processUpdate;
	if(!data.games)
		data.games = {};
	if(!data.players)
		data.players = {};
	
	updateGames();
	scanInactivity();
	timer.addEvent(43200000,true,scanInactivity);	
	log(LOG_INFO,"Dicewarz II background service initialized");
}

/* shutdown service */
function close() {
	client.unsubscribe(game_id,"games");
	client.unsubscribe(game_id,"players");
	log(LOG_INFO,"terminating dicewarz2 background service");
}

/* main loop */
function main() {
	while(!js.terminated && !parent_queue.poll()) {
		if(client.socket.poll(.5))
			client.cycle();
		timer.cycle();
	}
}

try {

open();
main();
close();

} catch(e) {
	log(LOG_ERROR,e.toSource());
}