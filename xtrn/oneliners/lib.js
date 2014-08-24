load("json-client.js");

var initSettings = function(path) {
	var f = new File(path + "settings.ini");
	f.open("r");
	var o = f.iniGetObject();
	f.close();
	return o;
}

var postOneliner = function(alias, userInput) {
	try {
		oneliners.post(alias, userInput);
	} catch(err) {
		log(LOG_ERR, "JSON client error: " + err);
		exit();
	}
}

var Oneliners = function(server, port, callback) {

	var jsonClient = new JSONClient(server, port);

	this.__defineGetter__(
		'count',
		function() {
			return jsonClient.read("ONELINERS", "ONELINERS.length", 1);
		}
	);
	this.__defineSetter__('count', function() {});

	this.read = function(start, end) {
		return jsonClient.slice(
			"ONELINERS",
			"ONELINERS",
			start,
			(typeof end == "undefined") ? undefined : end,
			1
		);
	}

	this.post = function(alias, oneliner) {
		var obj = {
			'time' : time(),
			'client' : (typeof client != "undefined") ? client.ip_address : system.inet_addr,
			'alias' : alias,
			'systemName' : system.name,
			'systemHost' : system.inet_addr,
			'qwkid' : system.qwk_id.toLowerCase(),
			'oneliner' : oneliner
		};
		jsonClient.push(
			"ONELINERS",
			"ONELINERS",
			obj,
			2
		);
	}

	this.cycle = function() {
		jsonClient.cycle();
	}

	this.close = function() {
		jsonClient.disconnect();
	}

	if(typeof callback == "function") {
		var handler = function(update) {
			if(update.location != "LATEST" || update.oper != "WRITE")
				return;
			callback(update.data);
		}
		jsonClient.callback = handler;
		jsonClient.subscribe("ONELINERS", "LATEST");
	}

}
