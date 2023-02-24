require('sbbsdefs.js', 'P_UTF8');
const wttr = load({}, js.exec_dir + 'wttr-lib.js');

function main() {
	const ansi = wttr.getWeather();
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