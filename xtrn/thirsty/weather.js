// Thirstyville weather routines
// echicken -at- bbs.electronicchicken.com

var celsiusToFahrenheit = function(temp) {
	temp = ((temp * 9) / 5) + 32;
	return temp;
}

var fahrenheitToCelsius = function(temp) {
	temp = ((temp - 32) * 5) / 9;
	return temp;
}

var weatherConditions;

var makeWeather = function() {
	var weather = {};
	try {
		var f = new File(js.exec_dir + "weather.ini");
		f.open("r");
		weatherConditions = f.iniGetAllObjects();
		f.close();
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
	for(var day = 1; day <=7; day++) {
		var condition = Math.floor(Math.random()*weatherConditions.length);
		var POP = Math.floor(Math.random() * weatherConditions[condition].maximumPOP);
		if(POP < weatherConditions[condition].minimumPOP)
			POP = weatherConditions[condition].minimumPOP;
		if(weatherConditions[condition].minimumTemperature < 1)
			var offset = weatherConditions[condition].minimumTemperature - (weatherConditions[condition].minimumTemperature * 2);
		else
			var offset = 0;
		var temp = Math.floor((Math.random() * (weatherConditions[condition].maximumTemperature + offset)) + 1) - offset;
		if(temp < weatherConditions[condition].minimumTemperature)
			temp = weatherConditions[condition].minimumTemperature;
		weather[day] = {
			'name' : weatherConditions[condition].name,
			'graphic' : weatherConditions[condition].graphic,
			'POP' : POP.toFixed(),
			'temperature' : temp
		};
	}
	return weather;
}

var getWeather = function(update) {
	var weather = {};
	try {
		weather = jsonClient.read("THIRSTY", "THIRSTY.WEATHER", 1);
		if(weather === undefined || update) {
			weather = makeWeather();
			jsonClient.write("THIRSTY", "THIRSTY.WEATHER", weather, 2);
		}
		return weather;
	} catch(err) {
		log(LOG_ERR, err);
		return false;
	}
}

var dummy = function() {
}

var makeWeatherList = function() {
	weatherTab.getcmd = dummy;
	weatherTab.frame.clear();
	if(player.day > 7) {
		weatherTab.frame.putmsg("Data unavailable.  Come back tomorrow!", WHITE);
		return;
	}
	weatherTab.frame.load(js.exec_dir + "graphics/weather-" + weather[player.day].graphic + ".bin", 8, 4);
	weatherTab.frame.gotoxy(10, 1);
	weatherTab.frame.putmsg(weather[player.day].name, WHITE);
	weatherTab.frame.gotoxy(10, 2);
	weatherTab.frame.putmsg(format("Probability of Precipitation:  %s\%", weather[player.day].POP), WHITE);
	weatherTab.frame.gotoxy(10, 3);
	weatherTab.frame.putmsg(
		format(
			"Temperature: %s%sC (%s%sF)",
			weather[player.day].temperature.toFixed(),
			ascii(248),
			Math.round(celsiusToFahrenheit(weather[player.day].temperature)),
			ascii(248)
		),
		WHITE
	);
}