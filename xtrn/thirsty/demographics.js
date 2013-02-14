// Thirstyville demographics routines
// echicken -at- bbs.electronicchicken.com

var makeDemographics = function() {
	try {
		var demographics = {};
		var fileName = js.exec_dir + "demographics.ini";
		if(!file_exists(fileName))
			throw "demographics.ini not present.";
		var fileHandle = new File(fileName);
		fileHandle.open("r");
		var demographicsIni = fileHandle.iniGetAllObjects();
		fileHandle.close();
		var percent = 0;
		for(var section in demographicsIni) {
			var percentage = Math.floor(
				(Math.random() * parseInt(demographicsIni[section].maxPercent))
				+
				parseInt(demographicsIni[section].minPercent)
			);
			percent = percent + percentage;
			if(percent > 100)
				throw "Demographic percentages incorrect.";
			var ages = demographicsIni[section].ages.split(",");
			var incomes = demographicsIni[section].incomes.split(",");
			var averageIncome = Math.floor(
				(Math.random() * parseInt(incomes[1])) + parseInt(incomes[0])
			);
			demographics[md5_calc(demographicsIni[section].name, true)] = {
				'name' : demographicsIni[section].name,
				'percentage' : percentage,
				'youngest' : parseInt(ages[0]),
				'oldest' : parseInt(ages[1]),
				'averageIncome' : averageIncome,
				'coldDrinks' : demographicsIni[section].coldDrinks.split(","),
				'hotDrinks' : demographicsIni[section].hotDrinks.split(",")
			};
		}
		if(percent < 100) {
			demographics.vagrants = {
				'name' : "Vagrants",
				'percentage' : 100 - percent,
				'youngest' : "Unknown",
				'oldest' : "Unknown",
				'averageIncome' : 0,
				'coldDrinks' : true,
				'hotDrinks' : true
			};
		}
		return demographics;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var getDemographics = function(update) {
	var demographics = {};
	try {
		demographics = jsonClient.read("THIRSTY", "THIRSTY.DEMOGRAPHICS", 1);
		if(demographics === undefined || update)
			demographics = makeDemographics();
		else
			return demographics;
		if(!demographics)
			return false;
		jsonClient.write("THIRSTY", "THIRSTY.DEMOGRAPHICS", demographics, 2);
		return demographics;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var showMarketReport = function() {
	marketTab.frame.clear();
	marketTab.frame.putmsg(
		format(
			"%-20s %-20s %-20s %s\r\n",
			"Segment", "Ages", "Population", "Average Income"
		),
		DARKGRAY
	);
	for(var property in demographics) {
		marketTab.frame.putmsg(
			format(
				"%-20s %-20s %-20s $%s\r\n",
				demographics[property].name,
				demographics[property].youngest + "-" + demographics[property].oldest,
				(gameSettings.population * (demographics[property].percentage/100)).toFixed(),
				demographics[property].averageIncome
			),
			LIGHTGRAY
		);
	}
	marketTab.frame.scrollTo(0, 0);
	makeStats();
}

var buyMarketReport = function(cmd) {
	switch(cmd) {
		case KEY_UP:
			marketTab.frame.scroll(0, -1);
			break;
		case KEY_DOWN:
			marketTab.frame.scroll(0, 1);
			break;
		default:
			break;
	}
	if(cmd.toUpperCase() != "Y")
		return;
	if(player.marketReport >= gameSettings.week || player.money <= gameSettings.reportCost || player.day > 7)
		return;
	player.marketReport = gameSettings.week;
	player.money = player.money - gameSettings.reportCost;
	jsonClient.write("THIRSTY", "THIRSTY.PLAYERS." + playerID, player, 2);
	showMarketReport();
}

var getMarketReport = function() {
	marketTab.frame.clear();
	if(player.marketReport < gameSettings.week && player.day <= 7)
		marketTab.frame.putmsg("Press [Y] to purchase a market research report for $" + gameSettings.reportCost + ".", LIGHTCYAN);
	else
		showMarketReport();
}