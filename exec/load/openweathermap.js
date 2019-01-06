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
    this.cache = {};
    this.requests = [];
}

OpenWeatherMap.prototype.write_cache = function (endpoint, params, response) {
    const hash = base64_encode(endpoint + JSON.stringify(params));
    this.cache[hash] = { time: time(), data: response };
}

OpenWeatherMap.prototype.read_cache = function (endpoint, params) {
    const hash = base64_encode(endpoint + JSON.stringify(params));
    if (!this.cache[hash]) return;
    // This is probably not the right way, but it'll do
    if (time() - this.cache[hash].time > this.settings.data_refresh) return;
    return this.cache[hash].data;
}

OpenWeatherMap.prototype.rate_limit = function () {
    const now = time();
    this.requests = this.requests.filter(function (e) {
        return now - e < this.settings.rate_window;
    }, this);
    if (this.requests.length < this.settings.rate_limit) {
        this.requests.push(now);
        return true;
    } else {
        return false;
    }
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
