// Thirstyville products routines
// echicken -at- bbs.electronicchicken.com

var makeProducts = function() {
	try {
		var products = {};
		var fileName = js.exec_dir + "products.ini";
		if(!file_exists(fileName))
			throw "products.ini not present.";
		var fileHandle = new File(fileName);
		fileHandle.open("r");
		var productsIni = fileHandle.iniGetAllObjects();
		fileHandle.close();
		for(var section in productsIni) {
			products[md5_calc(productsIni[section].name, true)] = {
				'name' : productsIni[section].name,
				'ingredients' : productsIni[section].ingredients.split(","),
				'dollarsPerIncomeFigure' : parseFloat(productsIni[section].dollarsPerIncomeFigure).toFixed(2),
				'category' : productsIni[section].category
			};
		}
		return products;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var getProducts = function(update) {
	var products = {};
	try {
		products = jsonClient.read("THIRSTY", "THIRSTY.PRODUCTS", 1);
		if(products === undefined || update)
			products = makeProducts();
		else
			return products;
		if(!products)
			return false;
		jsonClient.write("THIRSTY", "THIRSTY.PRODUCTS", products, 2);
		return products;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var showProductInfo = function(product) {
	inputFrame.top();
	inputFrame.clear();
	inputFrame.putmsg("Product information: " + player.products[product].name, WHITE);
	inputFrame.gotoxy(1, 3);
	inputFrame.putmsg("Requires the following ingredients: ", WHITE);
	inputFrame.gotoxy(1, 4);
	inputFrame.putmsg(products[product].ingredients.join(", "), LIGHTRED);
	inputFrame.gotoxy(1, 6);
	inputFrame.putmsg("< Press any key >", LIGHTCYAN);
	frame.cycle();
	console.getkey(K_NOSPIN);
	inputFrame.bottom();
}

var mixDrinks = function(product, cost, max) {
	if(player.day > 7)
		return;
	inputFrame.top();
	inputFrame.clear();
	inputFrame.putmsg("Product Preparation", WHITE);
	inputFrame.gotoxy(1, 3);
	inputFrame.putmsg(
		format(
			"%-16s %-8s %-8s %s",
			 "Item", "Cost", "Maximum", "Ingredients"
		),
		LIGHTGRAY
	);
	inputFrame.gotoxy(1, 4);
	inputFrame.putmsg(
		format(
			"%-16s %-8s %-8s %s",
			player.products[product].name,
			cost.toFixed(2),
			max,
			products[product].ingredients.join(", ")
			
		),
		WHITE
	);
	inputFrame.gotoxy(1, 6);
	inputFrame.putmsg("How many? -> ", WHITE);
	inputFrame.gotoxy(1, 7);
	inputFrame.putmsg("   Price? -> ", WHITE);
	frame.cycle();
	console.gotoxy(15, 19);
	var qty = parseInt(console.getstr("", 8, K_LINE));
	console.gotoxy(15, 20);
	var price = parseFloat(console.getstr("0.00", 8, K_EDIT|K_LINE));
	if(!isNaN(qty) && qty <= max && !isNaN(price) && price > 0) {
		console.gotoxy(2, 22);
		if(
			console.yesno(
				format(
					"Prepare %s units of %s at a cost of $%s",
					qty, player.products[product].name, (qty * cost).toFixed(2)
				)
			)
		) {
			player.products[product].quantity = player.products[product].quantity + qty;
			player.products[product].price = price;
			for(var ingredient in products[product].ingredients) {
				var stockProp = md5_calc(products[product].ingredients[ingredient], true);
				for(var p in player.inventory[stockProp]) {
					if(p == "quantity")
						player.inventory[stockProp][p] = player.inventory[stockProp][p] - qty;
				}
			}
			jsonClient.write("THIRSTY", "THIRSTY.PLAYERS." + playerID, player, 2);
		}
		makeStockList();
		makeMenu();
	}
	inputFrame.bottom();
	frame.invalidate();
}

var makeMenu = function() {
	if(menuTree instanceof Tree) {
		menuTree.close();
		menuView.delTab(0);
	}
	menuTree = new Tree();
	var ti = menuTree.addItem(
		format(
			"%-16s %-6s %-9s %s",
			"Product", "Qty", "Price", "Cost/ea."
		),
		true
	);
	ti.disable();
	for(var product in player.products) {
		var productCost = 0;
		var max = false;
		var disabled = false;
		for(var ingredient in products[product].ingredients) {
			// This is hugely convoluted, but I can't seem to access the
			// 'cost' property of a given stock item directly, eg:
			// player.inventory[md5_calc(products[product].ingredients[ingredient], true)].cost
			// even if what happens below *seems* effectively the same
			var stockProp = md5_calc(products[product].ingredients[ingredient], true);
			for(var p in player.inventory[stockProp]) {
				if(p == "cost")
					productCost = productCost + player.inventory[stockProp][p];
				if(p == "quantity" && player.inventory[stockProp][p] < 1)
					disabled = true;
				if(p == "quantity" && (!max || max > player.inventory[stockProp][p]))
					max = player.inventory[stockProp][p];
			}
		}
		ti = menuTree.addItem(
			format(
				"%-16s %-6s $%-8s $%s",
				player.products[product].name,
				player.products[product].quantity,
				(disabled && player.products[product].quantity < 1) ? "0.00" : parseFloat(player.products[product].price).toFixed(2),
				(disabled && player.products[product].quantity < 1) ? "0.00" : productCost.toFixed(2)
			),
			(disabled) ? showProductInfo : mixDrinks, product, productCost, max
		);
		if(disabled)
			ti.disable();
	}
	menuTree.colors.lfg = LIGHTCYAN;
	menuTree.colors.lbg = BG_CYAN;
	menuTree.colors.dfg = LIGHTGRAY;
	menuTree.colors.fg = WHITE;
	menuTree.open();
	menuTree.down();
	var t = menuView.addTab("tree", "tree", menuTree);
}