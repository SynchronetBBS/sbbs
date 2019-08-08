/*	Lemons JSON DB module service script
	This is loaded automatically by the JSON service, and runs in the
	background. */

// Loop forever
js.branch_limit = 0;
js.time_limit = 0;

// The JSON service passes the module's working directory as argv[0]
var root = argv[0];

load("sbbsdefs.js");
load("json-client.js");
load("event-timer.js");
load(root + "defs.js");

// We'll need a JSON client handle in various functions within this script
var jsonClient,
	timer,
	levelTime = 0;

// This will be called when we receive an update to a valid location
function updateScores(data) {

	// Make sure that the data provided is sane
	if (typeof data.player != "string" ||
		typeof data.system != "string" ||
		typeof data.level != "number" ||
		typeof data.score != "number"
	) {
		return;
	}
	
	// Make a unique ID / valid property name for the player
	data.uid = base64_encode(data.player + "@" + data.system, true);
	data.date = time();
	
	// Grab the current high scores list
	var hs = jsonClient.read(DBNAME, "SCORES.HIGH", 1);
	var changed = false; // We'll toggle this if we need to overwrite the list

	/*	If this player's score is higher than any score in the list, shove
		it into the list before that score. */
	for (var s = 0; s < hs.length; s++) {
		if(data.score < hs[s].score) continue;
		hs.splice(s, 0, data);
		changed = true;
		break;
	}

	/*	If this player's score is the lowest in the list, but the list isn't
		fully populated, tack this record onto the end of the list. */
	if (!changed && hs.length < 20) {
		hs.push(data);
		changed = true;
	}

	//	If we flagged a DB update, then update the DB.
	if (changed) {
		while(hs.length > 20) hs.pop();
		jsonClient.write(DBNAME, "SCORES.HIGH", hs, 2);
	}

	// In any event, we want to update this player's record
	updatePlayer(data);

}

// This will be called via updateScores
function updatePlayer(data) {

	// Read the player's record from the DB
	var player = jsonClient.read(DBNAME, "PLAYERS." + data.uid, 1);

	// Or populate said record if it doesn't exist
	if (!player) {
		jsonClient.write(
			DBNAME,
			"PLAYERS." + data.uid,
			{ 'LEVEL' : data.level, 'SCORES' : [] },
			2
		);
	/*	If the player has reached a new level, update their level number so
		they can start there next time. */
	} else if (player.LEVEL < data.level) {
		jsonClient.write(
			DBNAME, "PLAYERS." + data.uid + ".LEVEL", data.level, 2
		);
	}

	/*	Tack this score onto the player's 'scores' array.  We're not doing
		anything with this data at the moment, but we could. */
	jsonClient.push(
		DBNAME,
		"PLAYERS." + data.uid + ".SCORES",
		{ 'level' : data.level, 'score' : data.score, 'date' : data.date },
		2
	);

}

/*	We'll set this as the JSON client's callback function, and it will be
	called when an update to a subscribed location is received. */
function processUpdate(update) {

	// We're not interested in any updates that aren't writes
	if (update.oper != "WRITE") return;

	// Additionally, the update must be to a *.LATEST location
	var location = update.location.split(".");
	if (location.length != "2" || location[1] != "LATEST") return;

	switch (location[0].toUpperCase()) {
		// We're only subscribing to SCORES.LATEST right now
		case "SCORES":
			updateScores(update.data);
			break;
		// But maybe we'll want to watch this in the future
		case "PLAYERS":
			updatePlayer(update.data);
			break;
		default:
			break;
	}

}

/*	On startup, or if the local levels.json file has been updated recently,
	overwrite level data in the DB from the local file. */
function pushLevels() {

	if (levelTime == file_date(root + "levels.json")) return;
	
	levelTime = file_date(root + "levels.json");

	var f = new File(root + "levels.json");
	f.open("r");
	var levels = JSON.parse(f.read());
	f.close();

	jsonClient.write(DBNAME, "LEVELS", levels, 2);

}

// Set things up
function init() {

	// Load the server config if it exists, or fake it if not
	if (file_exists(root + "server.ini")) {
		var f = new File(root + "server.ini");
		f.open("r");
		var ini = f.iniGetObject();
		f.close();
		ini.port = parseInt(ini.port);
	} else {
		var ini = { 'host' : "127.0.0.1", 'port' : 10088 };
	}

	// Create our JSON client handle (declared outside this function)
	jsonClient = new JSONClient(ini.host, ini.port);

	// Subscribe to updates to SCORES.LATEST
	jsonClient.subscribe(DBNAME, "SCORES.LATEST");

	// If SCORES doesn't exist, create it with dummy data
	if (!jsonClient.read(DBNAME, "SCORES", 1)) {
		jsonClient.write(DBNAME, "SCORES", { 'LATEST' : {}, 'HIGH' : [] }, 2);
	}

	// If PLAYERS doesn't exist, create it with dummy data
	if (!jsonClient.read(DBNAME, "PLAYERS", 1)) {
		jsonClient.write(DBNAME, "PLAYERS", { 'LATEST' : {} }, 2);
	}

	// Overwrite levels in the DB with local data
	pushLevels();
	// And do so again if the local data is updated
	timer = new Timer();
	timer.addEvent(300000, true, pushLevels);

	// Call processUpdate when an update is received
	jsonClient.callback = processUpdate;

}

// Keep things rolling until we're told to stop
function main() {
	while (!js.terminated) {
		timer.cycle();
		jsonClient.cycle();
		mswait(50);
	}
}

// Clean things up
function cleanUp() { jsonClient.disconnect(); }

// Try to do things, log an error if necessary
try {
	init();
	main();
	cleanUp();
} catch(err) { }

// This is implied, but hey, why not?
exit();
