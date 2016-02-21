/*	bbsfinder.net updater for Synchronet BBS 3.15+
	echicken -at- bbs.electronicchicken.com */

load('sbbsdefs.js');
load('event-timer.js');
load('http.js');

var delay = 240000;

function loadSettings() {
	
	var f = new File(system.ctrl_dir + 'modopts.ini');
	f.open('r');
	if (!f.is_open) {
		throw 'Unable to open ' + system.ctrl_dir + 'modopts.ini for reading.'
	}
	opts = f.iniGetObject('bbsfinder');
	f.close();

	if (!opts.hasOwnProperty('username') || !opts.hasOwnProperty('password')) {
		throw 'BBSfinder account info could not be read from modopts.ini.'
	}

	opts.username = encodeURIComponent(opts.username);
	opts.password = encodeURIComponent(opts.password);

	return opts;

}

function update() {
	try {
		var opts = loadSettings();
		var ret = (new HTTPRequest()).Get(
			format(
				'http://www.bbsfinder.net/update.asp?un=%s&pw=%s',
				opts.username, opts.password
			)
		);
	} catch (e) {
		log(LOG_INFO, 'BBSfinder HTTP error: ' + e);
	}
	if(ret == 0) {
		log(LOG_INFO, 'BBSfinder update succeded.');
	} else {
		log(LOG_INFO, 'BBSfinder update failed.');
		log(LOG_DEBUG, ret);
	}
}

update();

if (argc > 0 && argv[0] === '-l') {
	var timer = new Timer();
	timer.addEvent(delay, true, update);
	while (!js.terminated) {
		timer.cycle();
		mswait(1000);
	}
}