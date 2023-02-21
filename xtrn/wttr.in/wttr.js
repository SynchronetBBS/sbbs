require('sbbsdefs.js', 'P_UTF8');
const wttr = load({}, js.exec_dir + 'wttr-lib.js');

function parseArgs() {
	const ret = {
		qs: 'https://wttr.in/?AFn',
		ttl: 3600, // Seconds
	};
	for (var n = 0; n < argc; n++) {
		const arg = parseInt(argv[n], 10);
		if (isNaN(arg)) {
			ret.qs = arg;
		} else {
			ret.ttl = arg;
		}
	}
	return ret;
}

function main() {
	const args = parseArgs();
	const ansi = wttr.getWeather(args.qs, args.ttl);
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