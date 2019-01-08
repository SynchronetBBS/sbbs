if (!js.global.get_geoip) js.global.load(js.global, "geoip.js");
if (!js.global.get_nicklocation) js.global.load(js.global, "nicklocate.js");
if (!js.global.OpenWeatherMap) js.global.load(js.global, 'openweathermap.js');

const owm = new OpenWeatherMap();

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
    return n + '\xB0C \1n\1r(\1h\1c' + owm.c_to_f(n) + '\xB0F\1n\1r)';
}
