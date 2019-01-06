// Uses OpenWeatherMap, openweathermap.org
// CC BY-SA 4.0 https://creativecommons.org/licenses/by-sa/4.0/
// Sign up for your account & API key here: https://openweathermap.org/api
// Add an [openweathermap] section to modopts.ini
// Add an api_key value to that section with your key
// rate_window, rate_limit, and data_refresh values can be added as well,
// but the defaults are suited to the free tier.

load('http.js');
load('modopts.js');

function OpenWeatherMap() {
    this.settings = get_mod_options('openweathermap');
    if (!this.settings.rate_window) this.settings.rate_window = 60; // Seconds
    if (!this.settings.rate_limit) this.settings.rate_limit = 60; // Requests per window
    if (!this.settings.data_refresh) this.settings.data_refresh = 7200; // Seconds
    this.cache_file = new File(system.data_dir + 'openweathermap_cache.json');
    this.rate_file = new File(system.data_dir + 'openweathermap_rate.json');
}

OpenWeatherMap.prototype.write_cache = function (endpoint, params, response) {
    const hash = base64_encode(endpoint + JSON.stringify(params));
    if (!this.cache_file.exists) {
        var cache = {};
        cache[hash] = { time: time(), data: response };
        this.cache_file.open('w');
        this.cache_file.write(JSON.stringify(cache));
        this.cache_file.close();
    } else {
        this.cache_file.open('r+');
        var cache = JSON.parse(this.cache_file.read());
        cache[hash] = { time: time(), data: response };
        this.cache_file.truncate();
        this.cache_file.write(JSON.stringify(cache));
        this.cache_file.close();
    }
}

OpenWeatherMap.prototype.read_cache = function (endpoint, params) {
    if (!this.cache_file.exists) return;
    this.cache_file.open('r');
    const cache = JSON.parse(this.cache_file.read());
    this.cache_file.close();
    const hash = base64_encode(endpoint + JSON.stringify(params));
    if (!cache[hash]) return;
    // This is probably not the right way, but it'll do
    if (time() - cache[hash].time > this.settings.data_refresh) return;
    return cache[hash].data;
}

OpenWeatherMap.prototype.rate_limit = function () {
    const now = time();
    if (!this.rate_file.exists) {
        this.rate_file.open('w');
        var rate = [];
    } else {
        this.rate_file.open('r+');
        var rate = JSON.parse(this.rate_file.read());
    }
    rate = rate.filter(function (e) {
        return now - e < this.settings.rate_window;
    }, this);
    var ret = false;
    if (rate.length < this.settings.rate_limit) {
        rate.push(now);
        ret = true;
    }
    this.rate_file.truncate();
    this.rate_file.write(JSON.stringify(rate));
    this.rate_file.close();
    return ret;
}

OpenWeatherMap.prototype.call_api = function (endpoint, params) {

    const cache = this.read_cache(endpoint, params);
    if (cache) return cache;

    if (!this.rate_limit()) return { error: 'Rate limit exceeded' };

    var url = 'http://api.openweathermap.org/data/2.5/' + endpoint;
    url += Object.keys(params).reduce(function (a, c, i) {
        return a + (i == 0 ? '?' : '&') + c + '=' + params[c];
    }, '');
    url += '&APPID=' + this.settings.api_key;

    const response = JSON.parse((new HTTPRequest()).Get(url));
    this.write_cache(endpoint, params, response);

    return response;
}
