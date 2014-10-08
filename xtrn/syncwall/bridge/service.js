/*	SyncWall v1-v2 bridging service
	by echicken, September 2014

	Maintains backward compatibility for original (2012 version) SyncWall
	clients connecting to a system hosting a newer (2014 version) SyncWall
	database.
	
	If you want to host a SyncWall database and offer compatibility to older
	client versions, your 'ctrl/json-service.ini' should contain these
	sections, in this order:

	[syncwall2]
	dir=../xtrn/syncwall

	[syncwall]
	dir=../xtrn/syncwall/bridge
*/

load("json-client.js");
load("event-timer.js");

var jsonClient, timer, users, systems, month;

var host = "localhost",
	port = 10088,
	sw1DB = "SYNCWALL",
	sw2DB = "SYNCWALL2";

/*	SyncWall 2 shuffles monthly history out of the database and into separate
	files.  SyncWall 1 just kept it in the DB, causing bloat that eventually
	exceeded JSON DB's filesize limit.  By only storing the current month in
	our SW1 DB, we should be able to avoid the problems that came along with
	the old method.
*/
var newMonth = function() {

	var thisMonth = strftime("%m%y");
	if(thisMonth == month)
		return;

	log(LOG_INFO, "SyncWall bridging service performing monthly maintenance.");

	jsonClient.write(sw1DB, "canvas." + month, {}, 2);

	month = thisMonth;

}

var processSW1Update = function(update) {

	var location = update.location.split(".");
	if(location.length < 3 || location[2] != "ch")
		return;

	var data = update.data;
	// Drop any update that doesn't seem valid:
	if(typeof data.x != "number" || data.x < 0)
		return;
	if(typeof data.y != "number" || data.y < 0)
		return;
	if(typeof data.userAlias != "string" || data.userAlias.length < 1 || data.userAlias.length > 25)
		return;
	if(typeof data.system != "string" || data.system.length < 1 || data.system.length > 50)
		return;
	if(typeof data.ch != "string" || (data.ch.length > 1 && data.ch != "CLEAR"))
		return;
	if(typeof data.attr != "number" || data.attr < 0)
		return;

	if(users.indexOf(data.userAlias) < 0) {
		jsonClient.push(sw2DB, "USERS", data.userAlias, 2);
		users = jsonClient.read(sw2DB, "USERS", 1);
	}
	if(systems.indexOf(data.system) < 0) {
		jsonClient.push(sw2DB, "SYSTEMS", data.system, 2);
		systems = jsonClient.read(sw2DB, "SYSTEMS", 1);	
	}

	jsonClient.write(
		sw2DB,
		"LATEST",
		{	'x' : data.x - 1,
			'y' : data.y - 1,
			'c' : data.ch,
			'a' : data.attr,
			'u' : users.indexOf(data.userAlias),
			's' : systems.indexOf(data.system)
		},
		2
	);

}

var processSW2Update = function(update) {

	var data = update.data;

	if(update.location == "USERS" && users.indexOf(data) < 0) {
		users.push(data);
		return;
	}

	if(update.location == "SYSTEMS" && systems.indexOf(data) < 0) {
		systems.push(data);
		return;
	}

	/*	As you can see, SyncWall 1 was pretty retarded and insecure.
		SyncWall 2 has a bunch of permission-control and data validation
		handled on the server side.  SyncWall 1 left things fairly wide open,
		which was never a "problem" (BBS problem, that is,) but could have
		been. :D
	*/
	if(	update.location == "LATEST"
		&&
		data.u < users.length && data.u >= 0
		&&
		data.s < systems.length && data.s >= 0
		&&
		data.x <= 79
		&&
		data.y <= 21
	) {
		var thisMonth = strftime("%m%y");
		var location = "canvas." + thisMonth;
		var keys = jsonClient.keys(sw1DB, "canvas", 1);
		if(keys.indexOf(thisMonth) < 0)
			jsonClient.write(sw1DB, location, {}, 2);
		var ch = {
			'x' : data.x + 1,
			'y' : data.y + 1,
			'ch' : data.c,
			'attr' : data.a,
			'userAlias' : users[data.u],
			'system' : systems[data.s]
		};
		if(data.c == "CLEAR") {
			jsonClient.write(sw1DB, location + ".data", {}, 2);
		} else {
			jsonClient.write(
				sw1DB,
				location + ".data." + data.x + "." + data.y,
				{	'x' : ch.x,
					'y' : ch.y,
					'ch' : ch.ch,
					'attr' : ch.attr
				},
				2
			);
		}
		jsonClient.write(sw1DB,	location + ".ch", ch, 2);
		jsonClient.push(sw1DB, location + ".history", ch, 2);
	}

}

var processUpdate = function(update) {

	if(typeof update.location == "undefined" || update.oper != "WRITE")
		return;

	if(update.scope.toUpperCase() == sw1DB)
		processSW1Update(update);
	else if(update.scope.toUpperCase() == sw2DB)
		processSW2Update(update);

}

var init = function() {

	month = strftime("%m%y");

	jsonClient = new JSONClient(host, port);
	jsonClient.socket.debug_logging = true;
	jsonClient.callback = processUpdate;

	users = jsonClient.read(sw2DB, "USERS", 1);
	if(!users)
		users = [];

	systems = jsonClient.read(sw2DB, "SYSTEMS", 1);
	if(!systems)
		systems = [];

	var keys = jsonClient.keys(sw1DB, "canvas", 1);
	if(!keys) {
		jsonClient.write(sw1DB, "canvas", {}, 2);
	} else {
		for(var k = 0; k < keys.length; k++) {
			if(keys[k] != month)
				jsonClient.write(sw1DB, "canvas." + keys[k], {}, 2);
		}
	}
	jsonClient.subscribe(sw1DB, "canvas");

	jsonClient.subscribe(sw2DB, "USERS");
	jsonClient.subscribe(sw2DB, "SYSTEMS");
	jsonClient.subscribe(sw2DB, "LATEST");

	timer = new Timer();
	var event = timer.addEvent(3600000, true, newMonth);
	event.run();

}

var main = function() {

	while(!js.terminated) {
		timer.cycle();
		jsonClient.cycle();
		mswait(5);
	}

}

var cleanUp = function() {
	jsonClient.disconnect();
}

try {
	init();
	main();
	cleanUp();
	exit();
} catch(err) {
	log(LOG_ERR, "SyncWall 1 service error: " + err);
}
