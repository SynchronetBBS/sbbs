require('http.js', 'HTTPRequest');
const locator = load({}, js.exec_dir + 'locator.js');
const xterm = load({}, js.exec_dir + 'xterm-colors.js');

function uReplace(str) {
	return str.replace(/\xE2\x9A\xA1/g, '\x01+\x01h\x01yZ \x01-'); // U+26A1 Lightning bolt
}

function getCacheName(qs, addr) {
	const cfn = format('wttr.in_%s_%s.ans', qs, addr || '').replace(/[^0-9a-z\.]+/ig, '_');
	return system.temp_path + cfn;
}

function readCache(qs, addr, ttl) {
	const cfn = getCacheName(qs, addr);
	const f = new File(cfn);
	if (!f.exists) return;
	if (time() - file_date(cfn) > ttl) return;
	if (!f.open('r')) return;
	const cache = f.read();
	f.close();
	return cache;
}

function writeCache(qs, addr, ans) {
	const cfn = getCacheName(qs, addr);
	const f = new File(cfn);
	if (!f.open('w')) return;
	f.write(ans);
	f.close();
}

function fetchWeather(qs, addr) {
	const http = new HTTPRequest();
	if (addr !== undefined) http.extra_headers = { 'X-Forwarded-For': addr };
	const body = http.Get(qs);
	if (http.response_code !== 200) throw new Error('wttr.in response had status ' + http.response_code);
	return body;
}

function getWeather(qs, ttl) {
	const addr = locator.getAddress();
	const cachedWeather = readCache(qs, addr, ttl);
	if (cachedWeather !== undefined) return cachedWeather;
	const weather = fetchWeather(qs, addr);
	const text = uReplace(weather);
	const ansi = xterm.convertColors(text);
	writeCache(qs, addr, ansi);
	return ansi;
}

this;