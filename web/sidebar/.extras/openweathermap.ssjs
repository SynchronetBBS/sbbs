// Requires an API key from https://openweathermap.org/
// Add an [openweathermap] section to ctrl/modopts.ini
// Add an api_key value to that section
(function () {
    try {
        load('geoip.js');
        require('openweathermap.js', 'OpenWeatherMap');
        const geoip = get_geoip(
            http_request.header['x-forwarded-for'] || http_request.remote_ip
        );
        const owm = new OpenWeatherMap();
        const wq = { units: 'metric', mode: 'html' };
        if (geoip.latitude && geoip.longitude) {
            wq.lat = geoip.latitude;
            wq.lon = geoip.longitude;
        } else if (geoip.cityName) {
            wq.q = geoip.cityName;
        }
        writeln(owm.call_api('weather', wq, true).data);
    } catch (err) {
        // meh
    }
})();
