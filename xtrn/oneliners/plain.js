var settings, oneliners, attr;

var init = function() {
	oneliners = new Oneliners(settings.server, settings.port);
	attr = console.attributes;
	console.clear(BG_BLACK|LIGHTGRAY);
	console.putmsg("\1h\1wsynchronet oneliners");
	console.crlf(); console.crlf();
}

var getOneliners = function() {
	var count = oneliners.count;
	if(count > console.screen_rows - 4)
		var lines = oneliners.read(count - console.screen_rows - 4);
	else
		var lines = oneliners.read(0);
	for(var line in lines) {
		console.putmsg(
			format(
				"\1n\1w%s\1n\1c@\1h\1b%s\1h\1k: \1n\1w%s\r\n",
				lines[line].alias,
				lines[line].qwkid,
				pipeToCtrlA(
					(	lines[line].oneliner.length
						+
						lines[line].alias.length
						+
						lines[line].qwkid.length
						+
						3
						>
						console.screen_columns
					)
					?
					lines[line].oneliner.substr(
						0,
						console.screen_columns
						-
						(	lines[line].alias.length
							+
							lines[line].qwkid.length
							+
							3
						)
					)
					:
					lines[line].oneliner
				)
			)
		);
	}
}

var prompt = function() {
	console.crlf();
	return console.noyes("Post a oneliner to the wall? ");
}

var postOneliner = function() {
	console.clearline();
	console.putmsg(user.alias + "@" + system.qwk_id + ": ");
	var userInput = console.getstr(
		"",
		console.screen_columns - user.alias.length - system.qwk_id.length - 3,
		K_LINE|K_EDIT
	);
	if(console.strlen(userInput) < 1)
		return;
	oneliners.post(user.alias, userInput);
}

var cleanUp = function() {
	oneliners.close();
	console.attributes = attr;
	console.clear();
}

try {
	init();
	getOneliners();
	if(!prompt())
		postOneliner();
	cleanUp();
} catch(err) {
	log(LOG_ERR, "Oneliners error: " + err);
}