load("json-client.js");

// You should edit this shit
var host = "mybbs.synchro.net",
	port = 10088,
	dbName = "SYNCWALL",
	historyPath = "/sbbs/web/root/syncwall";

var jsonClient = new JSONClient(host, port);

var keys = jsonClient.keys(dbName, "canvas", 1);

for(var key = 0; key < keys.length; key++) {
	var month = {
		'MONTH' : keys[key].substr(0, 2) + "20" + keys[key].substr(2, 2),
		'USERS' : [],
		'SYSTEMS' : [],
		'SEQUENCE' : [],
		'STATE' : {}
	};
	var len = jsonClient.read(dbName, "canvas." + keys[key] + ".history.length", 1);
	for(var i = 0; i < len; i = i + ((len - i > 100) ? 100 : len - 1)) {
		var history = jsonClient.slice(dbName, "canvas." + keys[key] + ".history", i, i + ((len - i > 100) ? 100 : len - 1), 1);
		for(var h = 0; h < history.length; h++) {
			if(typeof history[h] == "undefined" || history[h] === null)
				continue;
			var u = month.USERS.indexOf(history[h].userAlias);
			if(u < 0) {
				month.USERS.push(history[h].userAlias);
				u = month.USERS.length - 1;
			}
			var s = month.SYSTEMS.indexOf(history[h].system);
			if(s < 0) {
				month.SYSTEMS.push(history[h].system);
				s = month.SYSTEMS.length - 1;
			}
			month.SEQUENCE.push(
				{	'x' : history[h].x,
					'y' : history[h].y,
					'u' : u,
					's' : s,
					'c' : history[h].ch,
					'a' : history[h].attr
				}
			);
		}
	}
	var data = jsonClient.read(dbName, "canvas." + keys[key] + ".data", 1);
	for(var x in data) {
		for(var y in data[x]) {
			if(typeof month.STATE[y] == "undefined")
				month.STATE[y] = {};
			month.STATE[y][x] = {
				'c' : data[x][y].ch,
				'a' : data[x][y].attr
			}
		}
	}
	var f = new File(historyPath + "/" + month.MONTH + ".json");
	f.open("w");
	f.write(JSON.stringify(month));
	f.close();
}
