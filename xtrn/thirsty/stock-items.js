// Thirstyville inventory routines
// echicken -at- bbs.electronicchicken.com

var makeStockItems = function() {
	try {
		var stockItems = {};
		var fileName = js.exec_dir + "stock-items.ini";
		if(!file_exists(fileName))
			throw "stock-items.ini not present.";
		var fileHandle = new File(fileName);
		fileHandle.open("r");
		var stockItemsIni = fileHandle.iniGetAllObjects();
		fileHandle.close();
		var players = jsonClient.keys("THIRSTY", "THIRSTY.PLAYERS", 1).length;
		for(var day = 1; day <= 7; day++) {
			stockItems[day] = {};
			for(var section in stockItemsIni) {
				var cost = parseFloat(
					((Math.random() * parseFloat(stockItemsIni[section].maximumCost))
					+ parseFloat(stockItemsIni[section].minimumCost)).toFixed(2)
				);
				var availability = Math.round(
					(parseInt(gameIni.population) * ((players > 0) ? Math.floor(players / 2) + 1 : 1)) * parseFloat(stockItemsIni[section].availability)
				);
				stockItems[day][md5_calc(stockItemsIni[section].name, true)] = {
					'name' : stockItemsIni[section].name,
					'cost' : cost,
					'availability' : availability
				};
			}
		}
		return stockItems;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var getStockItems = function(update) {
	var stockItems = {};
	try {
		stockItems = jsonClient.read("THIRSTY", "THIRSTY.STOCKITEMS", 1);
		if(stockItems === undefined || update)
			stockItems = makeStockItems();
		else
			return stockItems;
		if(!stockItems)
			return false;
		jsonClient.write("THIRSTY", "THIRSTY.STOCKITEMS", stockItems, 2);
		return stockItems;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var orderInventory = function(item) {
	if(player.day > 7)
		return;
	inputFrame.top();
	inputFrame.clear();
	inputFrame.putmsg("Order Entry", WHITE);
	inputFrame.gotoxy(1, 3);
	inputFrame.putmsg(
		format(
			"%-16s %-14s %-8s %s",
			 "Item", "Availability", "Cost", "Maximum Order"
		),
		LIGHTGRAY
	);
	inputFrame.gotoxy(1, 4);
	var maxOrder = Math.floor(player.money / stockItems[player.day][item].cost);
	inputFrame.putmsg(
		format(
			"%-16s %-14s $%-7s %s",
			stockItems[player.day][item].name,
			stockItems[player.day][item].availability,
			stockItems[player.day][item].cost.toFixed(2),
			((maxOrder > stockItems[player.day][item].availability) ? stockItems[player.day][item].availability : maxOrder)
		),
		WHITE
	);
	inputFrame.gotoxy(1, 6);
	inputFrame.putmsg("How many? -> ", WHITE);
	frame.cycle();
	console.gotoxy(15, 19);
	var qty = parseInt(console.getstr("", 8, K_LINE));
	if(!isNaN(qty) && qty <= stockItems[player.day][item].availability && qty * stockItems[player.day][item].cost <= player.money) {
		console.gotoxy(2, 21);
		if(
			console.yesno(
				format(
					"Order %s units of %s for a total of $%s",
					qty,
					stockItems[player.day][item].name,
					(qty * stockItems[player.day][item].cost).toFixed(2)
				)
			)
		) {
			var newCost = (
				(
					(player.inventory[item].quantity * player.inventory[item].cost)
					+
					(qty * stockItems[player.day][item].cost)
				)
				/
				(qty + player.inventory[item].quantity)
			);
			player.inventory[item].cost = newCost;
			player.inventory[item].quantity = player.inventory[item].quantity + qty;
			player.money = player.money - qty * stockItems[player.day][item].cost;
			jsonClient.write("THIRSTY", "THIRSTY.PLAYERS." + playerID, player, 2);
			jsonClient.write("THIRSTY", "THIRSTY.STOCKITEMS." + player.day + "." + item + ".availability", stockItems[player.day][item].availability - qty, 2);
			stockItems[player.day][item].availability = stockItems[player.day][item].availability - qty;
		}
		makeStockList();
		makeMenu();
		makeStats();
	}
	inputFrame.bottom();
	frame.invalidate();
}

var makeStockList = function() {
	if(stockTree instanceof Tree) {
		stockTree.close();
		stockView.delTab(0);
	}
	stockTree = new Tree();
	var ti = stockTree.addItem(
		format(
			"%-16s %-6s %s",
			"Item", "Qty", "Cost/ea."
		),
		true
	);
	ti.disable();
	for(var item in player.inventory) {
		stockTree.addItem(
			format(
				"%-16s %-6s $%s",
				player.inventory[item].name,
				player.inventory[item].quantity,
				(player.inventory[item].quantity > 0 ) ? parseFloat(player.inventory[item].cost).toFixed(2) : "0.00"
			),
			orderInventory,
			item
		);
	}
	stockTree.colors.lfg = LIGHTCYAN;
	stockTree.colors.lbg = BG_CYAN;
	stockTree.colors.dfg = LIGHTGRAY;
	stockTree.open();
	stockTree.down();
	var t = stockView.addTab("tree", "tree", stockTree);
}

var getStockCost = function() {
	var cost = 0;
	for(var item in player.inventory) {
		if(player.inventory[item].quantity > 0)
			cost = cost + (player.inventory[item].cost * player.inventory[item].quantity);
	}
	return cost;
}