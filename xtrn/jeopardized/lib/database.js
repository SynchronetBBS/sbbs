var Database = function (settings) {

	var self = this;
	var jsonClient;
	var callbacks = {}; // { 'location' : [ function (update) {} ] ... }
	var attempts = 0;

	function addUser(usr) {

		var id = self.getUserID(usr);

		var update = {
			key : 'users',
			data : {
				id : id,
				alias : usr.alias,
				system : system.name
			}
		};
		jsonClient.write(settings.dbName, 'input', update, 2);

		var u;
		for (var n = 0; n < settings.retries; n++) {
			mswait(settings.retryDelay);
			u = jsonClient.read(settings.dbName, 'users.' + id, 1);
			if (typeof u !== 'undefined') break;
		}

		return u;

	}

	function addUserGameState(usr, round) {

		var update = {
			key : 'game.users',
			data : { id : self.getUserID(usr) }
		};
		jsonClient.write(settings.dbName, 'input', update, 2);

		var ugs;
		for (var n = 0; n < settings.retries; n++) {
			mswait(settings.retryDelay);
			ugs = jsonClient.read(
				settings.dbName,
				'game.users.' + self.getUserID(usr),
				1
			);
			if (typeof ugs !== 'undefined') break;
		}

		return ugs;

	}

	this.getUserID = function (usr) {
		return base64_encode(usr.alias + '@' + system.name);
	}

	this.getState = function () {
		return jsonClient.read(settings.dbName, 'state', 1);
	}

	this.getRound = function (round) {
		var r = jsonClient.read(settings.dbName, 'game.round.' + round, 1);
		if (typeof r === 'object') r.number = round;
		return r;
	}

	this.getUser = function (usr) {
		var u = jsonClient.read(
			settings.dbName,
			'users.' + self.getUserID(usr),
			1
		);
		if (typeof u === 'undefined') return addUser(usr);
		return u;
	}

	this.getUserGameState = function (usr) {
		var ugs = jsonClient.read(
			settings.dbName,
			'game.users.' + self.getUserID(usr),
			1
		);
		if (typeof ugs === 'undefined') return addUserGameState(usr);
		return ugs;
	}

	this.markClueAsUsed = function (usr, round, category, clue) {
		var update = {
			key : 'game.state',
			data : {
				id : self.getUserID(usr),
				round : round,
				category : category,
				clue : clue
			}
		};
		return jsonClient.write(settings.dbName, 'input', update, 2);
	}

	this.submitAnswer = function (usr, round, category, clue, answer, wager) {
		var update = {
			key : 'game.answer',
			data : {
				id : self.getUserID(usr),
				round : round,
				category : category,
				clue : clue,
				answer : answer,
				value : wager
			}
		};
		return jsonClient.write(settings.dbName, 'input', update, 2);
	}

	this.reportClue = function (usr, clue, answer) {
		var update = {
			key : 'game.report',
			data : {
				userID : self.getUserID(usr),
				clueID : clue.id,
				userAnswer : answer,
				realAnswer : clue.answer
			}
		};
		return jsonClient.write(settings.dbName, 'input', update, 2);
	}

	this.nextRound = function (usr) {
		var update = {
			key : 'game.nextRound',
			data : {
				id : self.getUserID(usr)
			}
		};
		return jsonClient.write(settings.dbName, 'input', update, 2);
	}

	this.postMessage = function (message, usr) {
		var update = {
			key : 'messages',
			data : {
				alias : usr.alias,
				system : system.name,
				message : message,
				store : true
			}
		};
		return jsonClient.write(settings.dbName, 'input', update, 2);
	}

	this.notify = function (message, store) {
		var update = {
			key : 'messages',
			data : {
				system : system.name,
				store : store,
				message : message
			}
		}
		return jsonClient.write(settings.dbName, 'input', update, 2);
	}

	this.getMessages = function () {
		var messages = jsonClient.read(
			settings.dbName, 'messages.history', 1
		);
		return messages;
	}

	this.getNews = function (count) {
		if (typeof count === 'undefined') count = 50;
		return jsonClient.slice(settings.dbName, 'news', 0, 0 + count, 1);
	}

	this.getRankings = function () {
		var ret = { 
			today : { data : {}, money : [] }, 
			total : { data : {}, money : [] }
		};
		var today = jsonClient.read(settings.dbName, 'game.users', 1);
		var total = jsonClient.read(settings.dbName, 'users', 1);
		Object.keys(today).forEach(
			function (key) {
				var r = { 
					id : key,
					winnings : today[key].winnings,
					answers : today[key].answers
				};
				ret.today.money.push(key);
				ret.today.data[key] = r;
			}
		);
		Object.keys(total).forEach(
			function (key) {
				ret.total.money.push(key);
				ret.total.data[key] = {
					id : key,
					winnings : total[key].winnings,
					answers : total[key].answers
				};
			}
		);
		ret.today.money.sort(
			function (a, b) {
				return ret.today.data[b].winnings - ret.today.data[a].winnings;
			}
		);
		ret.total.money.sort(
			function (a, b) {
				return ret.total.data[b].winnings - ret.total.data[a].winnings;
			}
		);
		return ret;
	}

	this.who = function () {
		var who = jsonClient.who(settings.dbName, 'state');
		if (!who || !Array.isArray(who)) return [];
		return who;
	}

	this.on = function (location, callback) {
		if (typeof location !== 'string' || typeof callback !== 'function') {
			return -1;
		}
		if (typeof callbacks[location] === 'undefined') {
			jsonClient.subscribe(settings.dbName, location);
			callbacks[location] = [callback];
		} else {
			callbacks[location].push(callback);
		}
		return callbacks.length - 1;
	}

	this.off = function (location, index) {
		if (typeof location !== 'string' || typeof index !== 'number') return;
		if (typeof callbacks[location] === 'undefined') return;
		if (index < 0 || index > callbacks[location].length) return;
		callbacks[location][index] = null;
	}

	this.connect = function () {

		jsonClient = new JSONClient(settings.host, settings.port);

		Object.keys(callbacks).forEach(
			function (key) {
				jsonClient.subscribe(settings.dbName, key);
			}
		);

		jsonClient.callback = function (update) {
			log(JSON.stringify('update'));
			if (update.func !== 'UPDATE' || update.oper !== 'WRITE') return;
			if (!Array.isArray(callbacks[update.location])) return;
			callbacks[update.location].forEach(
				function (callback) {
					if (typeof callback !== 'function') return;
					callback(update);
				}
			);
		}

	}

	this.close = function () {
		Object.keys(callbacks).forEach(
			function (location) {
				jsonClient.unsubscribe(settings.dbName, location);
				callbacks[location].forEach(
					function (callback, index) {
						if (typeof callback === 'function') {
							self.off(location, index)
						}
					}
				);
			}
		);
	}

	this.cycle = function () {
		if (!jsonClient.connected) {
			throw 'Disconnected from server.';
		} else {
			jsonClient.cycle();
		}
	}

	this.__defineGetter__(
		'connected',
		function () {
			return jsonClient.connected;
		}
	);

	this.connect();

}