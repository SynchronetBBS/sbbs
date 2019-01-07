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
        if (params) {
            const res = owm.call_api('weather', params);
            var str = '\1n\1rWeather for \1h\1c' + res.name + '\1n\1r: '
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
        } else {

        }
    } catch (err) {
        log(LOG_DEBUG, 'Failed to display weather conditions: ' + err);
        srv.o(target, 'Failed to fetch weather conditions: ' + err);
    }

    return true;
}
