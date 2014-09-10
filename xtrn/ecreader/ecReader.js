load("sbbsdefs.js");
load("frame.js");
load("lib.js");

var frame, chooser, lister, reader, attr, systemStatus, settings;

var state = {
	listing : true,
	reading : false
};

// Well, parse it enough for our porpoises, anyway...
var parseCtrlA = function(str) {
	var arr = str.toUpperCase().split("\\1");
	var light = (arr.indexOf("H") > arr.indexOf("N"));
	var attr = 0;
	for(var a = 0; a < arr.length; a++) {
		switch(arr[a]) {
			case "K":
				attr |= ((light) ? DARKGRAY : BLACK);
				break;
			case "R":
				attr |= ((light) ? LIGHTRED : RED);
				break;
			case "G":
				attr |= ((light) ? LIGHTGREEN : GREEN);
				break;
			case "Y":
				attr |= ((light) ? YELLOW : BROWN);
				break;
			case "B":
				attr |= ((light) ? LIGHTBLUE : BLUE);
				break;
			case "M":
				attr |= ((light) ? LIGHTMAGENTA : MAGENTA);
				break;
			case "C":
				attr |= ((light) ? LIGHTCYAN : CYAN);
				break;
			case "W":
				attr |= ((light) ? WHITE : LIGHTGRAY);
				break;
			case "0":
				attr |= BG_BLACK;
				break;
			case "1":
				attr |= BG_RED;
				break;
			case "2":
				attr |= BG_GREEN;
				break;
			case "3":
				attr |= BG_BROWN;
				break;
			case "4":
				attr |= BG_BLUE;
				break;
			case "5":
				attr |= BG_MAGENTA;
				break;
			case "6":
				attr |= BG_CYAN;
				break;
			case "7":
				attr |= BG_LIGHTGRAY;
				break;
			default:
				break;
		}
	}
	return attr;
}

var initSettings = function() {
	var f = new File(js.exec_dir + "settings.ini");
	f.open("r");
	settings = f.iniGetObject();
	f.close();
	for(var s in settings) {
		if(typeof settings[s] == "string")
			settings[s] = parseCtrlA(settings[s]);
	}
}

var init = function() {

	attr = console.attributes;
	console.clear(LIGHTGRAY);

	systemStatus = bbs.sys_status;
	bbs.sys_status|=SS_MOFF;

	initSettings();
	
	frame = new Frame(
		1,
		1,
		console.screen_columns,
		console.screen_rows
	);
	frame.open();

	settings.frame = frame;

	settings.callback = function(sub, header, item) {
		reader = new Reader(
			{	'sub' : sub,
				'msg' : header.number,
				'frame' : frame,
				'fhc' : settings.fhc,
				'fvc' : settings.fvc
			}
		);
		state.reading = true;
		state.listing = false;
		log(JSON.stringify(item));
	}

	lister = new Lister(settings);

}

var main = function() {
	while(!js.terminated) {
		var userInput = console.inkey(K_NONE, 5);
		if(state.listing) {
			lister.cycle();
			if(!lister.getcmd(userInput))
				break;
		} else if(state.reading) {
			reader.cycle();
			if(!reader.getcmd(userInput)) {
				state.listing = true;
				state.reading = false;
				reader.close();
			}
		}
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
	}
}

var cleanUp = function() {
	lister.close();
	frame.close();
	console.clear(attr);
	bbs.sys_status = systemStatus;
}

init();
main();
cleanUp();
exit();
