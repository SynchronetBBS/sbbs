// Thirstyville
// echicken -at- bbs.electronicchicken.com

load("sbbsdefs.js");
load("json-client.js");
load("frame.js");
load("layout.js");
load("tree.js");
load("demographics.js");
load("products.js");
load("stock-items.js");
load("weather.js");
load("player.js");

var gameSettings, player, demographics, products, stockItems, weather, news;
var frame, splashFrame, gameFrame, statsFrame, helpFrame, introFrame;
var layout, menuView, stockView, consoleView;
var newsTab, weatherTab, marketTab;
var menuTree, stockTree;
var sysStatus, gameIni, jsonClient, playerID;

var processUpdate = function(update) {
	if(update.oper != "WRITE")
		return;
	var location = update.location.split(".");
	switch(location[1].toUpperCase()) {
		case "STOCKITEMS":
			stockItems[location[2]][location[3]][location[4]] = update.data; 
			break;
		case "NEWS":
			putNews(update.data[update.data.length - 1].message);
			break;
		default:
			return;
	}
}

var dataInit = function() {
	try {
		console.clear();

		var fileName = js.exec_dir + "game.ini";
		if(!file_exists(fileName))
			throw "game.ini not present.";
		var fileHandle = new File(fileName);
		fileHandle.open("r");
		gameIni = fileHandle.iniGetObject();
		fileHandle.close();

		jsonClient = new JSONClient(gameIni.server, gameIni.port);

		jsonClient.send({ scope:"ADMIN", func:"TIME" });
		var serverTime = jsonClient.wait();
		timeOffset = Number(serverTime) - time();

		var playerKeys = jsonClient.keys("THIRSTY", "THIRSTY.PLAYERS", 1);
		
		var update = false;
		gameSettings = jsonClient.read("THIRSTY", "THIRSTY.SETTINGS", 1);
		if(gameSettings === undefined) {
			update = true;
			gameSettings = {
				'updated' : time() + timeOffset,
				'population' : parseInt(gameIni.population) * ((playerKeys === undefined) ? 1 : playerKeys.length),
				'startingFunds' : parseInt(gameIni.startingFunds),
				'reportCost' : parseInt(gameIni.reportCost),
				'waterCost' : parseFloat(gameIni.waterCost),
				'electricityCost' : parseFloat(gameIni.electricityCost),
				'rentCost' : parseFloat(gameIni.rentCost),
				'week' : 1
			};
			var newsItem = [{ 
				'time' : time() + timeOffset,
				'message' : "New game started on " + system.timestr(time() + timeOffset) + "."
			}];
			jsonClient.write("THIRSTY", "THIRSTY.NEWS", newsItem, 2);
		} else if(strftime("%m-%d-%Y", time() + timeOffset) != strftime("%m-%d-%Y", gameSettings.updated)) {
			update = true;
			gameSettings.updated = time() + timeOffset;
			gameSettings.week++;
			var newsItem = {
				'time' : time() + timeOffset,
				'message' : format("Week %s has begun.", gameSettings.week)
			};
			jsonClient.push("THIRSTY", "THIRSTY.NEWS", newsItem, 2);
		}
		if(update)
			jsonClient.write("THIRSTY", "THIRSTY.SETTINGS", gameSettings, 2);
		
		news = [];
		var newsKeys = jsonClient.keys("THIRSTY", "THIRSTY.NEWS", 1);
		for(var k = ((newsKeys.length > 25) ? newsKeys.length - 25 : 0); k < newsKeys.length; k++) {
			news.push(jsonClient.read("THIRSTY", "THIRSTY.NEWS." + newsKeys[k], 1));
		}

		demographics = getDemographics(update);
		if(!demographics)
			throw "Unable to initialize demographics.";

		products = getProducts(update);
		if(!products)
			throw "Unable to initialize products.";

		stockItems = getStockItems(update);
		if(!stockItems)
			throw "Unable to initialize stock items.";

		weather = getWeather(update);
		if(!weather)
			throw "Unable to initialize weather.";
		
		player = getPlayer();
		if(!player)
			throw "Unable to initialize player.";
		
		jsonClient.callback = processUpdate;
		jsonClient.subscribe("THIRSTY", "THIRSTY.STOCKITEMS");
		jsonClient.subscribe("THIRSTY", "THIRSTY.NEWS");
		
		return true;
	} catch(err) {
		log(LOG_ERR, "Data initialization error: " + err);
		return false;
	}
}

var makeStats = function() {
	statsFrame.clear();
	statsFrame.putmsg(
		format(
			"Week %s, Day %s, Funds: $%s",
			gameSettings.week,
			(player.day > 7) ? 7 : player.day,
			player.money.toFixed(2)
		),
		LIGHTGREEN
	);
}

var getScores = function() {
	scoreTab.frame.clear();
	scoreTab.frame.putmsg(
		format(
			"%-25s %-35s %s\r\n",
			"Name", "System", "Money"
		),
		LIGHTCYAN
	);
	try {
		var keys = jsonClient.keys("THIRSTY", "THIRSTY.PLAYERS", 1);
		if(keys === undefined)
			throw "THIRSTY.PLAYERS has no properties.";
	} catch(err) {
		log(LOG_ERR, "Scoreboard error: " + err);
		return;
	}
	for(var k in keys) {
		var p = jsonClient.readmulti(
			[ 
				["THIRSTY", "THIRSTY.PLAYERS." + keys[k] + ".alias", 1, "alias"],
				["THIRSTY", "THIRSTY.PLAYERS." + keys[k] + ".system", 1, "system"],
				["THIRSTY", "THIRSTY.PLAYERS." + keys[k] + ".money", 1, "money"]
			]
		);
		scoreTab.frame.putmsg(
			format(
				"%-25s %-35s $%s\r\n",
				p.alias.substr(0, 24),
				p.system.substr(0, 34),
				p.money.toFixed(2)
			),
			WHITE
		);
	}
}

var graphicsInit = function() {
	try {
		frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK|WHITE);
		helpFrame = new Frame(9, 2, 63, 23, 0, frame);
		introFrame = new Frame(3, 4, 74, 18, 0, frame);
		splashFrame = new Frame(1, 1, 80, 24, 0, frame);
		gameFrame = new Frame(1, 1, frame.width, frame.height, BG_BLACK|WHITE, frame);
		statsFrame = new Frame(2, 24, 37, 1, BG_BLACK|WHITE, frame);
		inputFrame = new Frame(2, 14, 78, 9, BG_BLACK|WHITE, gameFrame);
		helpFrame.load(js.exec_dir + "graphics/help.bin", 63, 23);
		introFrame.load(js.exec_dir + "graphics/intro.bin", 74, 18);
		gameFrame.load(js.exec_dir + "graphics/game.bin", 80, 24);
		splashFrame.load(js.exec_dir + "graphics/thirsty.bin", splashFrame.width, splashFrame.height);
		splashFrame.move((frame - splashFrame.width) / 2, (frame - splashFrame.height) / 2);
		helpFrame.close();
		introFrame.close();

		layout = new Layout(gameFrame);
		layout.colors.border_fg = WHITE;
		layout.colors.title_bg = BG_BLUE;
		layout.colors.title_fg = LIGHTCYAN;
		layout.colors.tab_bg = BG_CYAN;
		layout.colors.tab_fg = LIGHTCYAN;
		layout.colors.inactive_tab_bg = BG_BLACK;
		layout.colors.inactive_tab_fg = CYAN;
		
		menuView = layout.addView("Your menu", 2, 2, 43, 10);
		menuView.show_tabs = false;
		menuView.show_border = false;
		makeMenu();

		stockView = layout.addView("Your inventory", 46, 2, 34, 10);
		stockView.show_tabs = false;
		stockView.show_border = false;
		makeStockList();

		consoleView = layout.addView("Management console", 2, 13, 78, 10);
		consoleView.show_tabs = true;
		consoleView.show_border = false;

		newsTab = consoleView.addTab("News", "frame");
		putNews("Welcome to Thirstyville!", WHITE);
		for(var n in news)
			putNews(news[n].message);
			
		weatherTab = consoleView.addTab("Weather Forecast", "frame");
		weatherTab.onEnter = makeWeatherList;

		marketTab = consoleView.addTab("Market Research", "frame");
		marketTab.onEnter = getMarketReport;
		marketTab.getcmd = buyMarketReport;
		
		scoreTab = consoleView.addTab("Scoreboard", "frame");
		scoreTab.onEnter = getScores;
		
		frame.open();
		layout.open();
		menuView.active = false;
		layout.current = 2;
		inputFrame.bottom();
		splashFrame.top();
		makeStats();
		frame.draw();
		return true;
	} catch(err) {
		log(LOG_ERR, "Graphics initialization error: " + err);
		return false;
	}
}

var promptFrame = function(showMe) {
	showMe.open();
	showMe.top();
	frame.cycle();
	console.gotoxy(console.screen_columns, console.screen_rows);
	console.getkey(K_NOSPIN);
	showMe.close();	
}

var putNews = function(message, attr) {
	if(newsTab.frame.data_height > newsTab.frame.height) {
		while(newsTab.frame.down()) { }
		newsTab.frame.gotoxy(1, newsTab.frame.height);
	}

	if(newsTab.frame.data_height > 0)
		newsTab.frame.putmsg("---\r\n", DARKGRAY);

	if(message instanceof Array) {
		for(var m = 0; m < message.length; m++)
			newsTab.frame.putmsg(
				word_wrap(message[m][0], 79).replace(/\n/g, "\r\n"),
				message[m][1]
			);
	} else {
		message = word_wrap(message, 79).replace(/\n/g, "\r\n");
		newsTab.frame.putmsg(message, attr);
	}

	newsTab.frame.scroll(0, -1);
	return;
}

var init = function() {
	sysStatus = bbs.sys_status;
	bbs.sys_status|=SS_MOFF;
	if(!dataInit() || !graphicsInit()) {
		log(LOG_ERR, "Thirstyville failed to start.");
		return false;
	}
	console.getkey(K_NOSPIN);
	splashFrame.close();
	splashFrame.delete();
	promptFrame(introFrame);
	return true;
}

var cycle = function() {
	try {
		jsonClient.cycle();
		layout.cycle();
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
		return true;
	} catch(err) {
		log(LOG_ERR, "Thirstyville exiting due to error: " + err);
		return false;
	}
}

var cleanUp = function() {
	if(jsonClient instanceof JSONClient) {
		jsonClient.unsubscribe("THIRSTY", "THIRSTY.STOCKITEMS");
		jsonClient.unsubscribe("THIRSTY", "THIRSTY.NEWS");
		jsonClient.disconnect();
	}
	if(frame instanceof Frame)
		frame.close();
	console.clear();
	bbs.sys_status = sysStatus;
}

var playTurn = function() {
	
	var report = [];
	
	menuView.active = false;
	stockView.active = false;
	layout.current = 2;
	weatherTab.active = false;
	marketTab.active = false;
	consoleView.current = 0;

	if(player.day > 7) {
		putNews(
			"You've taken all of your turns for today.  Continue the game tomorrow!",
			LIGHTCYAN
		);
		return false;
	}
	
	putNews("Play this turn? (Y/N)", LIGHTMAGENTA);
	frame.cycle();
	console.gotoxy(console.screen_columns, console.screen_rows);
	if(console.getkey(K_NOSPIN).toUpperCase() != "Y") {
		putNews("Turn canceled.", MAGENTA);
		return false;
	}
	
	report.push(
		[	format(
				"%s - Playing day %s ...",
				system.timestr(time() + timeOffset),
				player.day
			),
			WHITE
		]
	);

	var POP = 100 - weather[player.day].POP;
	var precipitation = false;
	var precipitationType = "";
	if(Math.floor((Math.random() * 100) + 1) < POP) {
		precipitation = true;
		precipitationType = (weather[player.day].temperature < 3) ? "snow" : "rain";
	}
	report.push(
		[	format(
				"Today's weather: %s%sC (%s%sF), %s.",
				weather[player.day].temperature.toFixed(),
				ascii(248),
				celsiusToFahrenheit(weather[player.day].temperature).toFixed(),
				ascii(248),
				(precipitation) ? precipitationType : "no precipitation"
			),
			WHITE
		]
	);

	var hdpi, cdpi;
	if(weather[player.day].temperature < -10) {
		hdpi = 80;
		cdpi = 20;
	} else if(weather[player.day].temperature < 5) {
		hdpi = 60;
		cdpi = 40;
	} else if(weather[player.day].temperature < 15) {
		hdpi = 50;
		cdpi = 50;
	} else if(weather[player.day].temperature < 25) {
		hdpi = 30;
		cdpi = 70;
	} else {
		hdpi = 20;
		cdpi = 80;
	}
	if(precipitation) {
		hdpi = hdpi + 20;
		cdpi = cdpi - 20;
	}

	var stockCosts = 0;
	var water = 0;
	var electricity = 0;
	for(var product in player.products) {
		if(player.products[product].quantity < 1)
			continue;
		electricity = electricity + player.products[product].quantity;
		for(var ingredient in products[product].ingredients) {
			if(products[product].ingredients[ingredient] == "Water") {
				water = water + player.products[product].quantity;
				continue;
			}
			var stockProp = md5_calc(products[product].ingredients[ingredient], true);
			for(var p in player.inventory[stockProp]) {
				if(p != "cost")
					continue;
				stockCosts = stockCosts + (player.inventory[stockProp][p] * player.products[product].quantity);
				break;
			}
		}
	}
	
	var stockLosses = 0;
	var lpi = 100 - demographics.vagrants.percentage;
	if(Math.floor((Math.random() * 100) + 1) > lpi) {
		var losses = [];
		for(var product in player.products) {
			if(player.products[product].quantity < 2)
				continue;
			var loss = Math.floor((Math.random() * (player.products[product].quantity)) + 1);
			player.products[product].quantity = player.products[product].quantity - loss;
			losses.push(
				format(
					"%s unit%s of %s",
					loss,
					(loss > 1) ? "s" : "",
					player.products[product].name
				)
			);
		}
		for(var ingredient in player.inventory) {
			if(player.inventory[ingredient].quantity < 2)
				continue;
			var loss = Math.floor((Math.random() * (player.inventory[ingredient].quantity)) + 1);
			player.inventory[ingredient].quantity = player.inventory[ingredient].quantity - loss;
			losses.push(
				format(
					"%s unit%s of %s from storage",
					loss,
					(loss > 1) ? "s" : "",
					player.inventory[ingredient].name
				)
			);
			stockLosses = stockLosses + (player.inventory[ingredient].quantity * player.inventory[ingredient].cost);
		}
		if(losses.length > 0) {
			report.push(
				[	format(
						"Your shop was looted by vagrants!  You lost %s.",
						losses.join(", ")
					),
					LIGHTRED
				]
			);
			makeStockList();
		}
	}
	
	var storeClosed = false;
	var loseAllStock = false;
	if(Math.floor((Math.random() * 20) + 1) == 10) {
		storeClosed = true;
		if(precipitation && precipitationType == "snow") {
			report.push([
				"Blizzard conditions have kept all of the customers away!",
				LIGHTRED
			]);
		} else if(precipitation && precipitationType == "rain")	{
			loseAllStock = true;
			var repairCost = Math.floor(Math.random() * (player.money * 0.5)) + Math.floor(Math.random() * (player.money * 0.2));
			report.push([
				format(
					"Heavy rainfall has caused a flood!  Your inventory was destroyed, and you must pay $%s in clean-up costs.",
					repairCost.toFixed(2)
				),
				LIGHTRED
			]);
			player.money = player.money - repairCost;
		} else if(weather[player.day].temperature < -5) {
			loseAllStock = true;
			report.push([
				"The cold weather caused vagrants to take shelter in your shop, and they've stolen all of your inventory!",
				LIGHTRED
			]);
		} else {
			var n = Math.floor(Math.random() * 2) + 1;
			if(n == 1) {
				loseAllStock = true;
				var repairCost = Math.floor(Math.random() * (player.money * 0.8)) + Math.floor(Math.random() * (player.money * 0.1));
				report.push([
					format(
						"Your shop caught fire!  Your inventory was destroyed, and you must pay $%s in repair costs.",
						repairCost.toFixed(2)
					),
					LIGHTRED
				]);
				player.money = player.money - repairCost;
			} else if(n == 2) {
				report.push([
					"Vagrants have camped out on your doorstep!  Customers are too afraid to enter.",
					LIGHTRED
				]);
			}
		}
		if(loseAllStock) {
			for(var product in player.products)
				player.products[product].quantity = 0;
			for(var ingredient in player.inventory)
				player.inventory[ingredient].quantity = 0;
		}
		makeMenu();
		makeStockList();
	}
	
	var grossSales = 0;
	for(var segment in demographics) {

		if(storeClosed)
			break;

		var headCount = (gameSettings.population * (demographics[segment].percentage / 100)).toFixed();
		var hotDrinkBuyers = (headCount * (hdpi / 100)).toFixed();
		var coldDrinkBuyers = (headCount * (cdpi / 100)).toFixed();

		for(var d = 0; d < demographics[segment].hotDrinks.length; d++) {

			if(hotDrinkBuyers == 0)
				break;

			var drinkProperty = md5_calc(demographics[segment].hotDrinks[d], true);
			if(player.products[drinkProperty].quantity == 0) {
				var customerLoss = Math.floor(hotDrinkBuyers * 0.1);
				hotDrinkBuyers = hotDrinkBuyers - customerLoss;
				continue;
			}
			var expensive = products[drinkProperty].dollarsPerIncomeFigure * demographics[segment].averageIncome.toString().length;
			if(player.products[drinkProperty].price >= expensive)
				var percentage = (((Math.random() * 10) + 1) / 100).toFixed(2);
			else if(player.products[drinkProperty].price <= expensive / 2)
				var percentage = (((Math.random() * 30) + 70) / 100).toFixed(2);
			else
				var percentage = (((Math.random() * 60) + 20) / 100).toFixed(2);
			hotDrinkBuyers = Math.floor(hotDrinkBuyers - (hotDrinkBuyers * percentage));
			var unitsSold = Math.floor(player.products[drinkProperty].quantity * percentage);

			if(unitsSold > 0 && unitsSold <= player.products[drinkProperty].quantity) {
				var sales = (unitsSold * player.products[drinkProperty].price).toFixed(2);
				report.push(
					[	format(
							"%s %s bought $%s worth of %s.",
							unitsSold,
							(unitsSold > 1) ? demographics[segment].name : demographics[segment].name.replace(/s$/, ""),
							sales,
							demographics[segment].hotDrinks[d]
						),
						LIGHTGREEN
					]
				);
				player.products[drinkProperty].quantity = player.products[drinkProperty].quantity - unitsSold;
				player.money = player.money + parseFloat(sales);
				grossSales = grossSales + parseFloat(sales);
			}
		}
		
		for(var d = 0; d < demographics[segment].coldDrinks.length; d++) {

			if(coldDrinkBuyers === 0)
				break;

			var drinkProperty = md5_calc(demographics[segment].coldDrinks[d], true);
			if(player.products[drinkProperty].quantity == 0) {
				var customerLoss = Math.floor(coldDrinkBuyers * 0.1);
				coldDrinkBuyers = coldDrinkBuyers - customerLoss;
				continue;
			}
			var expensive = products[drinkProperty].dollarsPerIncomeFigure * demographics[segment].averageIncome.toString().length;
			if(player.products[drinkProperty].price >= expensive)
				var percentage = (((Math.random() * 10) + 1) / 100).toFixed(2);
			else if(player.products[drinkProperty].price <= expensive / 2)
				var percentage = (((Math.random() * 30) + 70) / 100).toFixed(2);
			else
				var percentage = (((Math.random() * 60) + 20) / 100).toFixed(2);
			coldDrinkBuyers = Math.floor(coldDrinkBuyers - (coldDrinkBuyers * percentage));
			var unitsSold = Math.floor(player.products[drinkProperty].quantity * percentage);
			
			if(unitsSold > 0 && unitsSold <= player.products[drinkProperty].quantity) {
				var sales = (unitsSold * player.products[drinkProperty].price).toFixed(2);
				report.push(
					[	format(
							"%s %s bought $%s worth of %s.",
							unitsSold,
							(unitsSold > 1) ? demographics[segment].name : demographics[segment].name.replace(/s$/, ""),
							sales,
							demographics[segment].coldDrinks[d]
						),
						LIGHTGREEN
					]
				);
				player.products[drinkProperty].quantity = player.products[drinkProperty].quantity - unitsSold;
				player.money = player.money + parseFloat(sales);
				grossSales = grossSales + parseFloat(sales);
			}
		}
	}
	
	for(var product in player.products) {

		if(player.products[product].quantity < 1)
			continue;

		var cost = 0;
		for(var ingredient in products[product].ingredients) {
			var stockProp = md5_calc(products[product].ingredients[ingredient], true);
			for(var p in player.inventory[stockProp]) {
				if(p == "cost")
					cost = cost + (player.inventory[stockProp][p] * player.products[product].quantity);
			}
		}

		report.push(
			[	format(
					"%s unit%s of %s had to be discarded, at a cost of $%s.",
					player.products[product].quantity,
					(player.products[product].quantity > 1) ? "s" : "",
					player.products[product].name,
					cost.toFixed(2)
				),
				LIGHTRED
			]
		);
		player.products[product].quantity = 0;
	}
	
	if(water > 0) {
		player.money = player.money - (water * gameSettings.waterCost);
		report.push(
			[	format(
					"The city of Thirstyville has charged you $%s for today's water usage.",
					(water * gameSettings.waterCost).toFixed(2)
				),
				LIGHTRED
			]
		);
	}
	
	if(electricity > 0) {
		player.money = player.money - (electricity * gameSettings.electricityCost);
		report.push(
			[	format(
					"The city of Thirstyville has charged you $%s for today's electricity usage.",
					(electricity * gameSettings.electricityCost).toFixed(2)
				),
				LIGHTRED
			]
		);
	}
		
	var overHeads = (electricity * gameSettings.electricityCost) + (water * gameSettings.waterCost);
	var totalCosts = stockCosts + overHeads;
	var grossProfit = grossSales - stockCosts;
	var operatingProfit = grossProfit - overHeads;
	var carryingCosts = getStockCost();
	
	var message = {
		'time' : time() + timeOffset,
		'message' :	format(
			"Week %s, Day %s\r\n%s had $%s in sales ($%s %s) and $%s in operating costs, for a net %s of $%s. $%s worth of inventory was carried over.",
			gameSettings.week,
			player.day,
			player.alias,
			grossSales.toFixed(2),
			grossProfit.toFixed(2),
			(grossProfit >= 0) ? "profit" : "loss",
			overHeads.toFixed(2),
			(operatingProfit >= 0) ? "profit" : "loss",
			operatingProfit.toFixed(2),
			carryingCosts.toFixed(2)
		)
	};
	report.push([message.message, LIGHTGREEN]);
	putNews(report);
	jsonClient.push("THIRSTY", "THIRSTY.NEWS", message, 2);
	
	if(player.day == 7 && player.weeks == 1) {
		player.money = player.money - gameSettings.startingFunds;
		putNews(
			"The bank collected on its $" + gameSettings.startingFunds + " loan.",
			LIGHTRED
		);
	}
	
	if(player.weeks % 4 == 0) {
		while(player.lastRentPayment < player.weeks) {
			player.lastRentPayment = player.lastRentPayment + 4;
			player.money = player.money - gameSettings.rentCost;
			putNews(
				format(
					"Your landlord collected $%s in rent for month %s.",
					gameSettings.rentCost, player.lastRentPayment
				),
				LIGHTRED
			);
			if(player.money < 0)
				break;
		}
	}
	
	player.day++;
	jsonClient.write("THIRSTY", "THIRSTY.PLAYERS." + playerID, player, 2);
	if(player.money <= 0) {
		putNews(
			"You went bankrupt! Your inventory has been repossessed.  Starting over ...",
			LIGHTRED
		);
		player = getPlayer();
		makeStockList();
	}
	if(player.day > 7) {
		putNews(
			"You've taken all of your turns for today.  Continue the game tomorrow!",
			LIGHTCYAN
		);
	}
		
	makeStats();
	makeMenu();
}

var main = function() {
	var userInput;
	while(!js.terminated) {
		userInput = console.inkey(K_NONE, 5);
		if(ascii(userInput) == 27)
			break;
		if(userInput.length > 0) {
			switch(userInput.toUpperCase()) {
				case "P":
					playTurn();
					break;
				case "I":
					promptFrame(introFrame);
					break;
				case "H":
					promptFrame(helpFrame);
					break;
				default:
					layout.getcmd(userInput);
					break;
			}
		}
		if(!cycle())
			break;
	}
}

if(init())
	main();
cleanUp();