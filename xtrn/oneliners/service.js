js.branch_limit = 0;
js.time_limit = 0;
var root = argv[0];
load(root + "lib.js");
load("json-client.js");

var settings, jsonClient, usr;

var processUpdate = function(update) {
	for(var count = jsonClient.read("ONELINERS", "ONELINERS.length", 1); count >= 50; count--) {
		jsonClient.push(
			"ONELINERS",
			"HISTORY",
			jsonClient.shift("ONELINERS", "ONELINERS", 2),
			2
		);
	}
	if(typeof update != "undefined" && update.oper == "WRITE") {
		jsonClient.write("ONELINERS", "LATEST", update.data[update.data.length - 1], 2);
	}
}

var init = function() {
	usr = new User(1);
	settings = initSettings(root);
	var dummy = [
		{	'time' : time(),
			'client' : system.inet_addr,
			'alias' : "oneliners",
			'systemName' : system.name,
			'systemHost' : system.inet_addr,
			'qwkid' : system.qwk_id.toLowerCase(),
			'oneliner' : "Oneliners database initialized."
		}
	];
	jsonClient = new JSONClient(settings.server, settings.port);
	jsonClient.ident("ADMIN", usr.alias, usr.security.password);
	if(!jsonClient.read("ONELINERS", "ONELINERS", 1)) {
		if(file_exists(root + "oneliners.json"))
			file_copy(root + "oneliners.json", root + "oneliners.bak");
		jsonClient.write("ONELINERS", "ONELINERS", dummy, 2);
		jsonClient.write("ONELINERS", "HISTORY", dummy, 2);
		jsonClient.write("ONELINERS", "LATEST", dummy[0], 2);
	}
	jsonClient.subscribe("ONELINERS", "ONELINERS");
	jsonClient.callback = processUpdate;
	processUpdate();
}

var main = function() {
	while(!js.terminated) {
		mswait(5);
		jsonClient.cycle();
	}
}

var cleanUp = function() {
	jsonClient.disconnect();
}

if(system.lastuser < 1)
	exit();

try {
	init();
	main();
	cleanUp();
	exit();
} catch(err) {
	log(LOG_ERR, "Oneliners service error: " + err);
}
