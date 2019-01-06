Bot_Commands["WEATHER"] = new Bot_Command(0,false,false);
Bot_Commands["WEATHER"].command = function (target,onick,ouh,srv,lvl,cmd) {

    var i;
    var lstr;
    var nick = onick;
    var res;
    var ll;

	// Remove empty cmd args
    for (i = 1; i < cmd.length; i++) {
        if (cmd[i].search(/^\s*$/) == 0) {
            cmd.splice(i, 1);
            i--;
        }
    }

    cmd.shift();
    if (cmd[0]) nick = cmd[0];

    if (typeof srv.users[nick.toUpperCase()] != 'undefined') {
	    try {
            lstr = get_nickcoords(srv.users[nick.toUpperCase()].uh, srv.users[nick.toUpperCase()].servername, nick);
            ll = lstr.match(/^(-*\d+\.\d+),(-*\d+\.\d+)$/);
        } catch (e) {
            log(LOG_DEBUG, 'Failed to get nick coordinates: ' + e);
        }
    }

    try {
        if (ll) {
            res = owm.call_api('weather', { lat: ll[1], lon: ll[2], units: 'metric' });
        } else {
            var usr = new User(system.matchuser(nick));
    		if (typeof usr == 'object' && usr.number > 0) {
                lstr = usr.location.split(',')[0];
            } else {
                lstr = nick; // Could be city name
            }
            usr = undefined;
            res = owm.call_api('weather', { q: lstr, units: 'metric' });
        }
        var str = 'Weather for ' + res.name + ': '
            + res.weather[0].main + ' (' + res.weather[0].description + '), '
            + res.clouds.all + '% cloudy, '
            + 'Current temp: ' + temperature_str(res.main.temp) + ', '
            + 'Min temp: ' + temperature_str(res.main.temp_min) + ', '
            + 'Max temp: ' + temperature_str(res.main.temp_max) + ', '
            + 'Wind: ' + res.wind.speed + ' KM/h ' + owm.wind_direction(res.wind.deg) + ', '
            + 'Humidity: ' + res.main.humidity + '%, '
            + 'Pressure: ' + res.main.pressure + ' hPa, '
            + 'Sunrise: ' + system.timestr(res.sys.sunrise) + ', '
            + 'Sunset: ' + system.timestr(res.sys.sunset);
        srv.o(target, str);
    } catch (err) {
        log(LOG_DEBUG, 'Failed to display weather conditions for ' + lstr + ': ' + err);
        srv.o(target, 'Failed to fetch weather conditions.');
    }

    return true;
}
