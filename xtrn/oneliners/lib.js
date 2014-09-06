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
		return oneliners.post(alias, userInput);
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
		var ret = [];
		var lines = jsonClient.slice(
			"ONELINERS",
			"ONELINERS",
			start,
			(typeof end == "undefined") ? undefined : end,
			1
		);
		while(lines.length > 0) {
			var line = lines.shift();
			if(typeof line == "undefined") // Probably a deleted oneliner
				continue;
			if(	typeof line.time != "number" ||
				typeof line.client != "string" ||
				typeof line.alias != "string" ||
				typeof line.systemName != "string" ||
				typeof line.systemHost != "string" ||
				typeof line.qwkid != "string" || line.qwkid.length > 8 ||
				typeof line.oneliner != "string" ||
				typeof strip_ctrl(line.oneliner) == "undefined" ||
				strip_ctrl(line.oneliner).length < 1
			) {
				continue;
			}
			ret.push(line);
		}
		return ret;
	}

	this.post = function(alias, oneliner) {
		var o = strip_ctrl(oneliner);
		if(typeof o == "undefined")
			return false;
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
		return obj;
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
