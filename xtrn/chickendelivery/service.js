js.branch_limit = 0;
js.time_limit = 0;
load("json-client.js");

var	root = argv[0],
	dbName = "CHICKENDELIVERY2",
	jsonClient;

var updateScores = function(data) {
	if(	typeof data.uid != "string"
		||
		typeof data.level != "number"
		||
		typeof data.score != "number"
		||
		typeof data.date != "number"
	) {
		return;
	}
	jsonClient.write(
		dbName,
		"SCORES." + data.uid,
		{ 'level' : data.level, 'score' : data.score, 'date' : data.date },
		2
	);
	var hs = jsonClient.read(dbName, "SCORES.HIGH",	1);
	var changed = false;
	if(hs.length < 20) {
		hs.push(data);
		changed = true;
	} else {
		for(var s = 0; s < hs.length; s++) {
			if(data.score < hs[s].score)
				continue;
			hs.splice(s, 0, data);
			changed = true;
			break;
		}
	}
	if(changed) {
		while(hs.length > 20)
			hs.pop();
		jsonClient.write(dbName, "SCORES.HIGH", hs, 2);
	}
	updatePlayer(data.uid, data.level);
}

var updatePlayer = function(uid, level) {
	var player = jsonClient.read(dbName, "PLAYERS." + uid, 1);
	if(!player)
		jsonClient.write(dbName, "PLAYERS." + uid, { 'level' : level }, 2);
	else if(player.level < level)
		jsonClient.write(dbName, "PLAYERS." + uid + ".level", level, 2);
}

var processUpdate = function(update) {
	if(update.oper != "WRITE")
		return;
	var location = update.location.split(".");
	if(location.length != 2)
		return;
	switch(location[0].toUpperCase()) {
		case "SCORES":
			updateScores(update.data);
			break;
		case "PLAYERS":
			updatePlayer(update.data);
			break;
		default:
			break;
	}
}

var initJson = function() {
	if(file_exists(root + "server.ini")) {
		var f = new File(root + "server.ini");
		f.open("r");
		var ini = f.iniGetObject();
		f.close();
	} else {
		var ini = { 'host' : "127.0.0.1", 'port' : 10088 };
	}
	var usr = new User(1);
	jsonClient = new JSONClient(ini.host, ini.port);
	jsonClient.subscribe(dbName, "SCORES.LATEST");
	if(!jsonClient.read(dbName, "SCORES", 1)) {
		jsonClient.write(
			dbName,
			"SCORES",
			{ 'LATEST' : {}, 'HIGH' : [] },
			2
		);
	}
	if(!jsonClient.read(dbName, "PLAYERS", 1)) {
		jsonClient.write(
			dbName,
			"PLAYERS",
			{ 'LATEST' : {} },
			2
		);
	}
	jsonClient.callback = processUpdate;
}

var main = function() {
	while(!js.terminated) {
		mswait(50);
		jsonClient.cycle();
	}
}

var cleanUp = function() {
	jsonClient.disconnect();
}

while(system.lastuser < 1)
	mswait(15000);

try {
	initJson();
	main();
	cleanUp();
} catch(err) {
//	log(LOG_ERR, err);
}

exit();
