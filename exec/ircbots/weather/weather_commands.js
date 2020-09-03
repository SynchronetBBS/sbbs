Bot_Commands["WEATHER"] = new Bot_Command(0,false,false);
Bot_Commands["WEATHER"].command = function (target, onick, ouh, srv, lvl, cmd) {

    // Remove empty cmd args
    for (var i = 1; i < cmd.length; i++) {
        if (cmd[i].search(/^\s*$/) == 0) {
            cmd.splice(i, 1);
            i--;
        }
    }
    cmd.shift();

    try {
        const params = get_params(cmd, onick, srv);
        if (!params) { 
			throw("error parsing parameters");
		}
		const res = owm.call_api('weather', params);
		if(!res || res.cod != 200) {
			throw(JSON.stringify(res));
		}
		var str = '\0010\1n\1rWeather for \1h\1c' + res.name + '\1n\1r: '
			+ '\1h\1c' + res.weather[0].main + ' \1n\1r(\1c' + res.weather[0].description + '\1r), '
			+ '\1h\1c' + res.clouds.all + '% cloudy\1n\1r, '
			+ 'Current temp: \1h\1c' + temperature_str(res.main.temp) + '\1n\1r, '
			+ 'Min temp: \1h\1c' + temperature_str(res.main.temp_min) + '\1n\1r, '
			+ 'Max temp: \1h\1c' + temperature_str(res.main.temp_max) + '\1n\1r, '
			+ 'Wind: \1h\1c' + res.wind.speed + ' KM/h ' + owm.wind_direction(res.wind.deg) + '\1n\1r, '
			+ 'Humidity: \1h\1c' + res.main.humidity + '%\1n\1r, '
			+ 'Pressure: \1h\1c' + res.main.pressure + ' hPa\1n\1r, '
			+ 'Sunrise: \1h\1c' + system.timestr(res.sys.sunrise) + '\1n\1r, '
			+ 'Sunset: \1h\1c' + system.timestr(res.sys.sunset)
			+ ' \1n\1m(\1h\1mProvided by OpenWeatherMap.org\1n\1m)';
		srv.o(target, ctrl_a_to_mirc(str));
    } catch (err) {
        log(LOG_DEBUG, 'Failed to display weather conditions: ' + err);
        srv.o(target, 'Failed to fetch weather conditions: ' + err);
    }

    return true;
}

Bot_Commands["FORECAST"] = new Bot_Command(0,false,false);
Bot_Commands["FORECAST"].command = function (target, onick, ouh, srv, lvl, cmd) {

    // Remove empty cmd args
    for (var i = 1; i < cmd.length; i++) {
        if (cmd[i].search(/^\s*$/) == 0) {
            cmd.splice(i, 1);
            i--;
        }
    }
    cmd.shift();

    try {
        const params = get_params(cmd, onick, srv);
        if (!params) { 
			throw("error parsing parameters");
		}
		const res = owm.call_api('forecast', params);
		if(!res || res.cod != 200) {
			throw(JSON.stringify(res));
		}
		
		var output = [];
		srv.o(target,ctrl_a_to_mirc('Forecast for \1h\1c' + res.city.name + ' \1n\1m(\1h\1mProvided by OpenWeatherMap.org\1n\1m)\1n\1r:'));
		//output.push('\0010\1n\1rForecast for \1h\1c' + res.city.name + ' \1n\1m(\1h\1mProvided by OpenWeatherMap.org\1n\1m)\1n\1r:');
		for(var d=0; d<res.list.length; d+=8) {
			var day_forecast = res.list.slice(d,d+8);
			if(day_forecast.length > 0) {
				output.push(get_day_forecast_rows(day_forecast));
			}
			writeln(d + ": " + JSON.stringify(day_forecast));
			// output.push(
				// '\0010\1h\1c ' + days[forecast_date.getDay()] + '\1n\1r, \1h\1c' + months[forecast_date.getMonth()] + ' ' + forecast_date.getDate() + '\1n\1r: '
				// + '\1h\1c' + day_forecast.weather[0].main + ' \1n\1r(\1c' + day_forecast.weather[0].description + '\1r), '
				// + '\1h\1c' + day_forecast.clouds.all + '% cloudy\1n\1r, '
				// + 'Min temp: \1h\1c' + temperature_str(day_forecast.main.temp_min) + '\1n\1r, '
				// + 'Max temp: \1h\1c' + temperature_str(day_forecast.main.temp_max) + '\1n\1r, '
				// + 'Wind: \1h\1c' + day_forecast.wind.speed + ' KM/h ' + owm.wind_direction(day_forecast.wind.deg) + '\1n\1r, '
				// + 'Humidity: \1h\1c' + day_forecast.main.humidity + '%\1n\1r, '
				// + 'Pressure: \1h\1c' + day_forecast.main.pressure + ' hPa'
			// );
		}
		// while(output.length > 0) {
			// srv.o(target, ctrl_a_to_mirc(output.shift()));
		// }
		for(var r=0;r<output[0].length;r++) {
			var row_string = "";
			for(var o=0;o<output.length;o++) {
				row_string += ctrl_a_to_mirc('\0010' + output[o][r]);
			}
			srv.o(target,row_string);
		}
		
    } catch (err) {
        log(LOG_DEBUG, 'Failed to display weather conditions: ' + err);
        srv.o(target, 'Failed to fetch weather conditions: ' + err);
    }

    return true;
}

