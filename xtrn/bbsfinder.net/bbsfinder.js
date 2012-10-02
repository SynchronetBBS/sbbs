/*	bbsfinder.net updater for Synchronet BBS 3.15+
	echicken -at- bbs.electronicchicken.com */

load("sbbsdefs.js");
load("event-timer.js");
load("http.js");

var http = new HTTPRequest();
var timer = new Timer();
var delay = 240000;
var opts;

function init() {
	var f = new File(system.ctrl_dir + "modopts.ini");
	f.open("r");
	if(!f.is_open) {
		log(LOG_INFO, "Unable to open " + system.ctrl_dir + "modopts.ini for reading.");
		return false;
	}
	opts = f.iniGetObject("bbsfinder");
	f.close();
	if(!opts.hasOwnProperty('username') || !opts.hasOwnProperty('password')) {
		log(LOG_INFO, "BBSfinder account info could not be read from " + system.ctrl_dir + "modopts.ini.");
		return false;
	}
	return true;
}

function update() {
	try {
		var ret = http.Get("http://www.bbsfinder.net/update.asp?un=" + opts.username + "&pw=" + opts.password);
	}
	catch(e) {
		log(LOG_INFO, "BBSfinder HTTP error: " + e);
		return false;
	}
	if(ret == 0) {
		log(LOG_INFO, "BBSfinder update succeded.");
		return true;
	} else {
		log(LOG_INFO, "BBSfinder update failed.");
		return false;
	}
}

if(!init())
	exit();
	
var r = update();
if(argc > 0 && argv[0] == "-l") {
	timer.addEvent(delay, true, update);
	while(!js.terminated) {
		timer.cycle();
		mswait(1000);
	}
}