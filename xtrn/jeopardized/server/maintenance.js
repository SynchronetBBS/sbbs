load('sbbsdefs.js');
load('http.js');
load('json-client.js');

var category_count = 5,
	clue_count = 5;

var settings = {},
	jsonClient,
	today = strftime('%d-%m-%Y');

function getCategoryIDs() {
	var IDs = JSON.parse(
		(new HTTPRequest()).Get(settings.WebAPI.url + '/categories')
	);
	if (!Array.isArray(IDs) || IDs.length < 1) throw 'Invalid API response';
	return IDs;
}

function getRandomCategory(IDs, round) {

	var details;
	var cc = round === 3 ? 1 : clue_count;

	while (typeof details === 'undefined') {
		var id = IDs[Math.floor(Math.random() * IDs.length)];
		var d = JSON.parse(
			(new HTTPRequest()).Get(settings.WebAPI.url + '/category/' + id)
		);
		if (d.clues[round - 1].length >= cc) {
			details = d;
			details.clues = [];
		} else {
			mswait(25);
		}
	}

	var clues = JSON.parse(
		(new HTTPRequest()).Get(
			settings.WebAPI.url + '/category/' + id + '/' + round
		)
	).filter(
		function (clue) {
			return (clue.answer.search(/[^\x20-\x7f]/) < 0);
		}
	);

	if (clues.length < cc) return;

	var chosen = [];
	while (details.clues.length < cc) {
		var clue = Math.floor(Math.random() * clues.length);
		if (chosen.indexOf(clue) < 0) {
			chosen.push(clue);
			details.clues.push(clues[clue]);
			details.clues[details.clues.length - 1].value =
				(details.clues.length * 100) * round;
			details.clues[details.clues.length - 1].dd = false;
		}
	}

	return details;

}

function getRound(IDs, round) {
	var categories = [];
	var cc = round === 3 ? 1 : category_count;
	while (categories.length < cc) {
		var category = getRandomCategory(IDs, round);
		if (typeof category !== 'undefined') categories.push(category);
	}
	var ddc1 = Math.floor(Math.random() * cc);
	var ddc2 = Math.floor(Math.random() * cc);
	var ddcc1 = Math.floor(Math.random() * cc);
	var ddcc2 = Math.floor(Math.random() * cc);
	categories[ddc1].clues[ddcc1].dd = true;
	categories[ddc2].clues[ddcc2].dd = true;
	return categories;
}

function backupObj(obj, date, name) {
	if (!file_isdir(js.exec_dir + 'backups')) mkdir(js.exec_dir + 'backups');
	var f = new File(js.exec_dir + 'backups/' + date + '-' + name + '.json');
	f.open('w');
	f.write(JSON.stringify(obj));
	f.close();
}

function backupYesterday() {

	var state = jsonClient.read(settings.JSONDB.dbName, 'state', 1);
	var game = jsonClient.read(settings.JSONDB.dbName, 'game', 1);
	var users = jsonClient.read(settings.JSONDB.dbName, 'users', 1);
	var messages = jsonClient.read(settings.JSONDB.dbName, 'messages', 1);
	var news = jsonClient.read(settings.JSONDB.dbName, 'news', 1);

	if (!state || !game || !users || !messages || !news) return true;

	backupObj(game, state.date, 'game');
	backupObj(users, state.date, 'users');
	backupObj(messages, state.date, 'messages');
	backupObj(news, state.date, 'news');

	return (state.date !== today);

}

function processYesterday() {

	var state = jsonClient.read(settings.JSONDB.dbName, 'state', 1);
	var gameUsers = jsonClient.read(settings.JSONDB.dbName, 'game.users', 1);

	if (!gameUsers) return;

	var money = [];
	var answers = [];

	Object.keys(gameUsers).forEach(
		function (key) {
			money.push({ id : key, money : gameUsers[key].winnings });
			answers.push(
				{ id : key, answers : gameUsers[key].answers.correct }
			);
		}
	);

	money.sort(
		function (a, b) {
			return b.money - a.money;
		}
	);

	answers.sort(
		function (a, b) {
			return b.answers - a.answers;
		}
	);

	if (money.length > 3) money.splice(3, money.length - 3);

	if (answers.length > 3) answers.splice(3, answers.length - 3);

	if (money.length > 0 || answers.length > 0) {
		var news = '\1h\1wStandings for \1c' + state.date + ':\r\n';
	}

	if (money.length > 0) {
		news += '\t\1gWinnings:\r\n';
		money.forEach(
			function (e, i, a) {
				var us = base64_decode(e.id).split('@');
				news += '\t\t\1y#' + (i + 1) + ': ' +
					' \1c' + us[0] + ' \1wwith \1g$' + e.money;
				if (i !== a.length - 1) news += '\r\n';
			}
		);
	}

	if (answers.length > 0) {
		news += '\r\n\t\1gCorrect answers:\r\n';
		answers.forEach(
			function (e, i, a) {
				var us = base64_decode(e.id).split('@');
				news += '\t\t\1y#' + (i + 1) + ': ' +
					' \1c' + us[0] + ' \1wwith \1g' + e.answers;
				if (i !== a.length - 1) news += '\r\n';
			}
		);
	}

	if (typeof news !== 'undefined') {
		jsonClient.push(settings.JSONDB.dbName, 'news', news, 2);
	}

}

function startNewDay() {

	var state = {
		locked : true,
		date : today
	};
	jsonClient.write(settings.JSONDB.dbName, 'state', state, 2);

	var IDs = getCategoryIDs();
	var game = {
		round : {
			'1' : getRound(IDs, 1),
			'2' : getRound(IDs, 2),
			'3' : getRound(IDs, 3)
		},
		users : {}
	};
	jsonClient.write(settings.JSONDB.dbName, 'game', game, 2);	

}

function loadSettings() {
	var f = new File(js.exec_dir + '../settings.ini');
	f.open('r');
	settings.JSONDB = f.iniGetObject('JSONDB');
	settings.WebAPI = f.iniGetObject('WebAPI');
	f.close();
}

function initJSON() {

	var usr = new User(1);

	jsonClient = new JSONClient(settings.JSONDB.host, settings.JSONDB.port);
	jsonClient.ident('ADMIN', usr.alias, usr.security.password.toUpperCase());

	var messages = jsonClient.read(settings.JSONDB.dbName, 'messages', 1);
	if (!messages) {
		jsonClient.write(
			settings.JSONDB.dbName,
			'messages',
			{ latest : {}, history : [] },
			2
		);
	} else if (messages.history.length > 50) {
		var keep = messages.history.slice(-50);
		messages.history = keep;
		jsonClient.write(settings.JSONDB.dbName, 'messages', messages, 2);
	}

	var news = jsonClient.read(settings.JSONDB.dbName, 'news', 1);
	if (!news) {
		jsonClient.write(settings.JSONDB.dbName, 'news', [], 2);
	} else if (news.length > 50) {
		var keep = news.slice(-50);
		news.history = keep;
		jsonClient.write(settings.JSONDB.dbName, 'news', news, 2);
	}

	if (!jsonClient.read(settings.JSONDB.dbName, 'users', 1)) {
		jsonClient.write(settings.JSONDB.dbName, 'users', {}, 2);
	}

	if (backupYesterday()) {
		processYesterday();
		startNewDay();
	}

}

function cleanUp() {
	jsonClient.write(settings.JSONDB.dbName, 'state.locked', false, 2);
}

try {
	loadSettings();
	initJSON();
} catch (err) {
	log(LOG_ERR, err);
} finally {
	cleanUp();
}