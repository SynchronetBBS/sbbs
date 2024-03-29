require('http.js', 'HTTPRequest');
const locator = load({}, js.exec_dir + 'locator.js');
const xterm = load({}, js.exec_dir + 'xterm-colors.js');

function parseArgs(settings, argv) {
	if (argv.length < 2) return settings;
	settings.fallback_ip = '';
	const keys = Object.keys(settings);
	for (var k = 0; k < keys.length && argv.length >= 2; k++) {
		const a = argv.shift().replace(/^--/, '');
		const v = argv.shift();
		if (settings[a] === undefined) log(LOG_WARNING, 'wttr.in: Invalid parameter ' + a);
		if (typeof settings[a] === 'number') {
			const n = parseInt(v, 10);
			if (isNaN(n)) throw new Error(a + ' must be a number, got ' + (typeof v) + ' v');
			settings[a] = n;
		} else {
			settings[a] = v;
		}
	}
	if (settings.fallback_ip === '') delete settings.fallback_ip;
	return settings;
}

function loadSettings(argv) {
	var settings = load({}, 'modopts.js', 'wttr.in') || {};
	if (settings.base_url === undefined) settings.base_url = 'https://wttr.in/';
	if (settings.units === undefined) settings.units = '';
	if (settings.view === undefined) settings.view = '1AFn';
	if (settings.cache_ttl === undefined) settings.cache_ttl = 3600;
	if (settings.fallback_location === undefined) settings.fallback_location = '';
	settings = parseArgs(settings, argv);
	if (settings.base_url.search(/\/$/) < 0) settings.base_url += '/';
	settings.fallback_location = settings.fallback_location.replace(/\s/g, '+');
	return settings;
}

function uReplace(str) {
	return str.replace(/\xE2\x9A\xA1/g, '\x01+\x01h\x01yZ \x01-'); // U+26A1 Lightning bolt
}

function getCacheName(url, addr) {
	const cfn = format('wttr.in_%s%s.ans', url, addr || '').replace(/[^0-9a-z\.]+/ig, '_');
	return system.temp_dir + cfn;
}

function readCache(url, addr, ttl) {
	const cfn = getCacheName(url, addr);
	const f = new File(cfn);
	if (!f.exists) return;
	if (time() - file_date(cfn) > ttl) return;
	if (!f.open('r')) return;
	const cache = f.read();
	f.close();
	return cache;
}

function writeCache(url, addr, ans) {
	const cfn = getCacheName(url, addr);
	const f = new File(cfn);
	if (!f.open('w')) return;
	f.write(ans);
	f.close();
}

function fetchWeather(url, addr) {
	const http = new HTTPRequest();
	if (addr !== undefined) http.extra_headers = { 'X-Forwarded-For': addr };
	const body = http.Get(url);
	if (http.response_code !== 200) throw new Error('wttr.in response had status ' + http.response_code);
	return body;
}

function getURL(settings, addr) {
	var url = settings.base_url;
	if (addr === undefined) url += settings.fallback_location;
	url += '?' + settings.units + settings.view;
	return url;
}

function getWeather(argv) {
	const settings = loadSettings(argv);
	const addr = locator.getAddress() || settings.fallback_ip;
	const url = getURL(settings, addr);
	if (settings.cache_ttl > 0) {
		const cachedWeather = readCache(url, addr, settings.cache_ttl);
		if (cachedWeather !== undefined) return cachedWeather;
	}
	const weather = fetchWeather(url, addr);
	const text = uReplace(weather);
	const ansi = xterm.convertColors(text);
	if (settings.cache_ttl > 0) writeCache(url, addr, ansi);
	return ansi;
}

const exports = {
	getWeather: getWeather,
};

exports;
