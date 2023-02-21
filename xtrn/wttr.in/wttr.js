require('sbbsdefs.js', 'P_UTF8');
require('http.js', 'HTTPRequest');
const xterm = load({}, js.exec_dir + 'xterm-colors.js');
const locator = load({}, js.exec_dir + 'locator.js');

function uReplace(str) {
	return str.replace(/\xE2\x9A\xA1/g, '/ '); // U+26A1 Lightning bolt
}

function fetchWeather(addr) {
	const qs = argc > 0 ? argv.join('') : 'AFn';
	const http = new HTTPRequest();
	if (addr !== undefined) http.extra_headers = { 'X-Forwarded-For': addr };
	const body = http.Get('https://wttr.in/?' + qs);
	if (http.response_code !== 200) throw new Error('wttr.in response had status ' + http.response_code);
	return body;
}

function main() {
	const addr = locator.getAddress();
	const weather = fetchWeather(addr);
	const text = uReplace(weather);
	const ansi = xterm.convertColors(text);
	const attr = console.attributes;
	console.clear(BG_BLACK|LIGHTGRAY);
	console.putmsg(ansi, P_UTF8);
	console.pause();
	console.attributes = attr;
}

try {
	main();
} catch (err) {
	log(LOG_ERROR, err);
}