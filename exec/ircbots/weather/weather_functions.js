if (!js.global.get_geoip) js.global.load(js.global, "geoip.js");
if (!js.global.get_nicklocation) js.global.load(js.global, "nicklocate.js");
if (!js.global.OpenWeatherMap) js.global.load(js.global, 'openweathermap.js');

var owm = new OpenWeatherMap();
var short_months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
var short_days = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"];

function locate_user(nick, srv) {
    const ret = { units: 'metric' };
    // If we have data about this IRC user
    if (typeof srv.users[nick.toUpperCase()] != 'undefined') {
        // Try geolocation
        var res = get_nicklocation(
            srv.users[nick.toUpperCase()].uh,
            srv.users[nick.toUpperCase()].servername,
            nick
        );
        // If we have coordinates
        if (res && (res.latitude != 0 || res.longitude != 0)) {
            ret.lat = res.latitude;
            ret.lon = res.longitude;
            return ret;
        }
    }
    // See if the user exists in the local DB
    var usr = new User(system.matchuser(nick));
    if (typeof usr == 'object' && usr.number > 0) {
        var loc = usr.location.split(',')[0];
        // Assume loc is a city name
        if (loc != '') {
            ret.q = loc;
            return ret;
        }
    }
    return; // We got nothin'
}

function get_params(cmd, nick, srv) {
    const ret = { units: 'metric' };
    // No parameters given, try to look up the user who issued the command
    if (cmd.length < 1) return locate_user(nick, srv);
    // Parameters were given
    if (cmd.length == 1) {
        // This could be a nick
        var res = locate_user(cmd[0], srv);
        if (res) return res;
        // If it's a ZIP code
        if (cmd[0].search(/^[0-9]{5}(?:-[0-9]{4})?$/) > -1) {
            ret.zip = cmd[0];
            ret.us = '';
            return ret;
        // If it's a Canadian postal code
        } else if (cmd[0].search(/^[A-Za-z]\d[A-Za-z]\d[A-Za-z]\d$/) > -1) {
            ret.zip = cmd[0];
            ret.ca = '';
            return ret;
        }
    }
    // Maybe a city name
    ret.q = cmd.join(' ').split(',')[0];
    return ret;
}

function temperature_str(n) {
    return Math.round(n) + '\xC2\xB0C \1n\1r(\1h\1c' + owm.c_to_f(n) + '\xC2\xB0F\1n\1r)';
}

function temperature_str_plain(n) {
    return Math.round(n) + '\xC2\xB0C (' + owm.c_to_f(n) + '\xC2\xB0F)';
}

function get_day_forecast_rows(forecasts) {
	var bar = [
		format_graph_row(get_forecast_day(forecasts),20),
		format_graph_row(get_avg_weather(forecasts),20),
		format_graph_row(get_low_temp(forecasts),26),
		format_graph_row(get_high_temp(forecasts),26),
		format_graph_row(get_avg_wind(forecasts),24)//,
		// format_graph_row(get_avg_humidity(forecasts),16),
		// format_graph_row(get_avg_pressure(forecasts),16)
	];
	return bar;
}

function get_low_temp(forecasts) {
	var low = undefined;
	for(var f=0;f<forecasts.length;f++) {
		var weather = forecasts[f].main;
		if(low === undefined || weather.temp_min < low) {
			low = weather.temp_min;
		}
	}
	return "\1n\1wL:\1h\1c " + temperature_str_plain(low);
}

function get_high_temp(forecasts) {
	var high = undefined;
	for(var f=0;f<forecasts.length;f++) {
		var weather = forecasts[f].main;
		if(high === undefined || weather.temp_max > high) {
			high = weather.temp_max;
		}
	}
	return "\1n\1wH:\1n\1r " + temperature_str_plain(high);
}

function get_forecast_day(forecasts) {
	var forecast_utc = forecasts[0].dt * 1000;
	var forecast_date = new Date(forecast_utc);
	var day_str = short_days[forecast_date.getDay()] + ', ' + short_months[forecast_date.getMonth()] + ' ' + forecast_date.getDate();
	return "\1n\1w" + day_str;
}

function get_avg_weather(forecasts) {
    var modeMap = {};
    var maxEl = forecasts[0], maxCount = 1;
    for(var i = 0; i < forecasts.length; i++)
    {
        var el = forecasts[i].weather[0].main;
        if(modeMap[el] == null)
            modeMap[el] = 1;
        else
            modeMap[el]++;  
        if(modeMap[el] > maxCount)
        {
            maxEl = forecasts[i];
            maxCount = modeMap[el];
        }
    }
    return "\1h\1y" + maxEl.weather[0].main;
}

function get_avg_wind(forecasts) {
	var total_wind = 0;
	var total_dir = 0;
	for(var f=0;f<forecasts.length;f++) {
		total_wind += forecasts[f].wind.speed;
		total_dir += forecasts[f].wind.deg
	}
	return "\1n\1wW:\1h\1y " + Math.round(total_wind/forecasts.length) + ' KM/h ' + owm.wind_direction(Math.round(total_dir/forecasts.length))
}

function get_avg_humidity(forecasts) {
	var total_hum = 0;
	for(var f=0;f<forecasts.length;f++) {
		total_hum += forecasts[f].main.humidity;
	}
	return "\1n\1wH:\1h\1y " + Math.round(total_hum/forecasts.length) + '%';
}

function get_avg_pressure(forecasts) {
	var total_press = 0;
	for(var f=0;f<forecasts.length;f++) {
		total_press += forecasts[f].main.pressure;
	}
	return "\1n\1wP:\1h\1y " + Math.round(total_press/forecasts.length) + ' hPa';
}

function format_graph_row(str, len) {
	return format("%-*s",len,str);
}
