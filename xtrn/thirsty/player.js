// Thirstyville player routines
// echicken -at- bbs.electronicchicken.com

playerID = md5_calc(user.alias.toUpperCase() + system.name.toUpperCase(), true);

var makePlayer = function() {
	player = {
		'alias' : user.alias,
		'system' : system.name,
		'money' : gameSettings.startingFunds,
		'day' : 1,
		'week' : gameSettings.week,
		'weeks' : 1,
		'lastRentPayment' : 0,
		'marketReport' : 0,
		'inventory' : {},
		'products' : {}
	};
	try {
		for(var item in stockItems[1]) {
			player.inventory[item] = {
				'name' : stockItems[1][item].name,
				'quantity' : 0,
				'cost' : 0
			};
		}
		for(var product in products) {
			player.products[product] = {
				'name' : products[product].name,
				'quantity' : 0,
				'price' : 0
			};
		}
		return player;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var getPlayer = function() {
	var player = {};
	try {
		player = jsonClient.read("THIRSTY", "THIRSTY.PLAYERS." + playerID, 1);
		if(player === undefined || player.money <= 0) {
			player = makePlayer();
			var message = {
				'time' : time() + timeOffset,
				'message' : format("%s@%s joined the game.", user.alias, system.name)
			};
			jsonClient.push("THIRSTY", "THIRSTY.NEWS", message, 2);
		} else if(player.week != gameSettings.week) {
			player.week = gameSettings.week;
			player.weeks++;
			player.day = 1;
		} else {
			return player;
		}
		if(!player)
			return false;
		jsonClient.write("THIRSTY", "THIRSTY.PLAYERS." + playerID, player, 2);
		return player;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var updatePlayer = function() {
	try {
		jsonClient.write("THIRSTY", "THIRSTY.PLAYERS." + playerID, player, 2);
		return true;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}