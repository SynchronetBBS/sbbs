js.branch_limit = 0;
js.time_limit = 0;
load("json-client.js");

var	root = argv[0],
	dbName = "GOOBLE2",
	jsonClient;

function updateScores(data) {
	if (typeof data.user != "string" ||
		typeof data.system != "string" ||
		typeof data.level != "number" ||
		typeof data.score != "number" ||
		typeof data.date != "number"
	) {
		return;
	}
	var hs = jsonClient.read(dbName, "SCORES.HIGH",	1);
	var changed = false;
	for (var s = 0; s < hs.length; s++) {
		if (data.score < hs[s].score) continue;
		hs.splice(s, 0, data);
		changed = true;
		break;
	}
	if ((!hs || hs.length < 20) && !changed) {
		hs.push(data);
		changed = true;
	}
	if (changed) {
		while (hs.length > 20) { hs.pop(); }
		jsonClient.write(dbName, "SCORES.HIGH", hs, 2);
	}
}

function processUpdate(update) {
	if (update.oper != "WRITE") return;
	var location = update.location.split(".");
	if (location.length != 2 ||
		location[0].toUpperCase() != "SCORES" ||
		location[1].toUpperCase() != "LATEST"
	) {
		return;
	}
	updateScores(update.data);
}

function initJson() {
	if (file_exists(root + "server.ini")) {
		var f = new File(root + "server.ini");
		f.open("r");
		var ini = f.iniGetObject();
		f.close();
	} else {
		var ini = { 'host' : "127.0.0.1", 'port' : 10088 };
	}
	jsonClient = new JSONClient(ini.host, ini.port);
	jsonClient.subscribe(dbName, "SCORES.LATEST");
	if (!jsonClient.read(dbName, "SCORES", 1)) {
		jsonClient.write(dbName, "SCORES", { 'LATEST' : {}, 'HIGH' : [] }, 2);
	}
	jsonClient.callback = processUpdate;
}

function main() {
	while (!js.terminated) {
		mswait(5);
		jsonClient.cycle();
	}
}

function cleanUp() { jsonClient.disconnect(); }

try {
	initJson();
	main();
	cleanUp();
} catch(err) { }

exit();
