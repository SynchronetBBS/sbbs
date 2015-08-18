load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("scrollbar.js");
load("funclib.js");
load("filebrowser.js");

Frame.prototype.drawBorder = function(color) {
	var theColor = color;
	if(Array.isArray(color));
		var sectionLength = Math.round(this.width / color.length);
	this.pushxy();
	for(var y = 1; y <= this.height; y++) {
		for(var x = 1; x <= this.width; x++) {
			if(x > 1 && x < this.width && y > 1 && y < this.height)
				continue;
			var msg;
			this.gotoxy(x, y);
			if(y == 1 && x == 1)
				msg = ascii(218);
			else if(y == 1 && x == this.width)
				msg = ascii(191);
			else if(y == this.height && x == 1)
				msg = ascii(192);
			else if(y == this.height && x == this.width)
				msg = ascii(217);
			else if(x == 1 || x == this.width)
				msg = ascii(179);
			else
				msg = ascii(196);
			if(Array.isArray(color)) {
				if(x == 1)
					theColor = color[0];
				else if(x % sectionLength == 0 && x < this.width)
					theColor = color[x / sectionLength];
				else if(x == this.width)
					theColor = color[color.length - 1];
			}
			this.putmsg(msg, theColor);
		}
	}
	this.popxy();
}

var root = js.exec_dir,
	settings,
	frame,
	browserFrame,
	statusBarFrame,
	areaFrame,
	speedFrame,
	pauseFrame;

var state = {
	'status' : bbs.sys_status,
	'attr' : console.attributes,
	'syncTerm' : false,
	'fileList' : [],
	'pausing' : false,
	'speed' : 0,
	'browser' : null
};

var speedMap = [
	0,		// 0 (unlimited)
	300,	// 1
	600,	// 2
	1200,	// 3
	2400,	// 4
	4800,	// 5
	9600,	// 6
	19200,	// 7
	38400,	// 8
	57600,	// 9
	76800,	// 10
	115200	// 11
];

var printFile = function(file) {

	console.clear(BG_BLACK|LIGHTGRAY);
	frame.invalidate();

	if(state.syncTerm) {

		console.putmsg("\x1B[0;" + state.speed + "*r");
		mswait(500);
		console.printfile(file, (state.pausing ? P_NONE : P_NOPAUSE));
		console.pause();
		console.putmsg("\x1B[0;0*r");

	} else if(state.speed == 0) {

		console.printfile(file, (state.pausing ? P_NONE : P_NOPAUSE));
		console.pause();

	} else {

		var f = new File(file);
		f.open("r");
		var contents = f.read().split("");
		f.close();
		
		var buf = Math.ceil((speedMap[state.speed] / 8) / 1000);

		if(!state.pausing) {
			bbs.sys_status&=(~SS_PAUSEON);
			bbs.sys_status|=SS_PAUSEOFF;
		}
		
		while(contents.length > 0) {
			console.write(contents.splice(0, buf).join(""));
			if(console.inkey(K_NONE, 1) != "")
				break;
		}
		console.pause();

		bbs.sys_status|=SS_PAUSEON;
		bbs.sys_status&=(~SS_PAUSEOFF);

	}

	console.clear(BG_BLACK|LIGHTGRAY);

}

// Basic check for SyncTERM; we don't really care which version it is
var isSyncTerm = function() {

	console.clear(BG_BLACK|LIGHTGRAY);
	console.write("Checking for SyncTERM ...");

	var ret = true,
		cTerm = "\x1B[=67;84;101;114;109;".split(""),
		ckpt = console.ctrlkey_passthru;

	console.ctrlkey_passthru = -1;
	console.write("\x1B[0c");

	while(cTerm.length > 0) {
		if(console.inkey(K_NONE, 5000) == cTerm.shift())
			continue;
		ret = false;
		break;
	}
	do {} while(console.inkey(K_NONE) != "") // Flush the input toilet

	console.ctrlkey_passthru = ckpt;
	return ret;

}

var showSpeed = function() {
	speedFrame.clear();
	speedFrame.putmsg(
		(state.speed == 0) ? "Full" : speedMap[state.speed]
	);
}

var showPause = function() {
	pauseFrame.clear();
	pauseFrame.putmsg((state.pausing) ? "On" : "Off");
}

var GalleryChooser = function() {

	var frames = {
		'frame' : null,
		'tree' : null,
		'scrollBar' : null
	};

	var getList = function() {
		var f = new File(root + "settings.ini");
		f.open("r");
		var galleries = f.iniGetAllObjects();
		f.close();
		return galleries;
	}

	this.open = function() {

		areaFrame.clear();
		areaFrame.putmsg("Gallery Menu");

		frames.frame = new Frame(
			browserFrame.x,
			browserFrame.y,
			browserFrame.width,
			browserFrame.height,
			browserFrame.attr,
			browserFrame
		);
		frames.tree = new Tree(frames.frame);
		frames.tree.colors.fg = getColor(settings.fg);
		frames.tree.colors.bg = getColor(settings.bg);
		frames.tree.colors.lfg = getColor(settings.lfg);
		frames.tree.colors.lbg = getColor(settings.lbg);

		var list = getList();
		list.forEach(
			function(item) {
				item.colors = settings;
				frames.tree.addItem(
					format("%-35s  %s", item.name, item.description),
					function() {
						state.browser.close();
						state.browser = load(root + item.module, JSON.stringify(item));
						state.browser.open();
						areaFrame.clear();
						areaFrame.putmsg(item.name);
					}
				);
			}
		);

		frames.scrollBar = new ScrollBar(frames.tree);

		frames.frame.open();
		frames.tree.open();

	}

	this.close = function() {
		frames.tree.close();
		frames.scrollBar.close();
		frames.frame.close();
	}

	this.cycle = function() {
		frames.scrollBar.cycle();
	}

	this.getcmd = function(cmd) {
		frames.tree.getcmd(cmd);
	}

}

var initDisplay = function() {

	console.clear(BG_BLACK|LIGHTGRAY);

	frame = new Frame(
		1,
		1,
		console.screen_columns,
		console.screen_rows,
		BG_BLACK|LIGHTGRAY
	);

	browserFrame = new Frame(
		frame.x + 1,
		frame.y + 1,
		frame.width - 2,
		frame.height - 3,
		frame.attr,
		frame
	);

	statusBarFrame = new Frame(
		frame.x + 1,
		frame.x + frame.height - 2,
		frame.width - 2,
		1,
		settings.sbg|settings.sfg,
		frame
	);
	statusBarFrame.putmsg(
		format(
			"%-41s%-16s%-13s",
			"Area:","S)peed:","P)ause:"
		)
	);
	statusBarFrame.gotoxy(statusBarFrame.width - 4, 1);
	statusBarFrame.putmsg("Q)uit");
	
	areaFrame = new Frame(
		statusBarFrame.x + 6,
		statusBarFrame.y,
		35,
		1,
		statusBarFrame.attr,
		statusBarFrame
	);

	speedFrame = new Frame(
		statusBarFrame.x + 49,
		statusBarFrame.y,
		6,
		1,
		statusBarFrame.attr,
		statusBarFrame
	);
	showSpeed();

	pauseFrame = new Frame(
		statusBarFrame.x + 65,
		statusBarFrame.y,
		3,
		1,
		statusBarFrame.attr,
		statusBarFrame
	);
	showPause();

	frame.drawBorder(settings.border);
	frame.gotoxy(frame.width - 24, 1);
	frame.putmsg(ascii(180) + "\1h\1kansiview by echicken\1h\1w" + ascii(195));

	frame.open();

}

var initSettings = function() {
	var f = new File(js.exec_dir + "settings.ini");
	f.open("r");
	settings = f.iniGetObject();
	f.close();
	settings.fg = getColor(settings.fg);
	settings.bg = getColor(settings.bg);
	settings.lfg = getColor(settings.lfg);
	settings.lbg = getColor(settings.lbg);
	settings.sfg = getColor(settings.sfg);
	settings.sbg = getColor(settings.sbg);
	settings.border = settings.border.split(",");
	settings.border.forEach(
		function(e, i, a) {
			a[i] = getColor(e);
		}
	);
}

var init = function() {

	bbs.sys_status|=(SS_MOFF|SS_PAUSEON);
	bbs.sys_status&=(~SS_PAUSEOFF);

	state.syncTerm = isSyncTerm();

	initSettings();
	initDisplay();

	state.browser = new GalleryChooser();
	state.browser.open();

}

var cleanUp = function() {
	frame.close();
	bbs.sys_status = state.status;
	console.attributes = state.attr;
	console.clear();
	exit(0);
}

var handleInput = function(userInput) {
	switch(userInput) {
		case "P":
			state.pausing = (state.pausing) ? false : true;
			showPause();
			break;
		case "S":
			state.speed = (state.speed + 1) % 12;
			showSpeed();
			break;
		case "Q":
			state.browser.close();
			frame.invalidate();
			if(state.browser instanceof GalleryChooser) {
				cleanUp();
			} else {
				state.browser = new GalleryChooser();
				state.browser.open();
			}
			break;
		default:
			state.browser.getcmd(userInput);
			break;
	}
}

var main = function() {

	while(!js.terminated) {
		handleInput(console.inkey(K_NONE, 5).toUpperCase());
		state.browser.cycle();
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
	}

}

init();
main();
cleanUp();
