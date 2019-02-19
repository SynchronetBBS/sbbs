load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("scrollbar.js");
load("funclib.js");
load("filebrowser.js");
var Ansi = load({}, "ansiterm_lib.js");
var Sauce = load({}, "sauce_lib.js");
var Graphic = load({}, "graphic.js");
var xbin = load({}, "xbin_lib.js");
var cterm = load({}, "cterm_lib.js");

Frame.prototype.drawBorder = function (color) {
    var msg;
	var theColor = color;
	const sectionLength = Array.isArray(color) ? Math.round(this.width / color.length) : 0;
	this.pushxy();
	for (var y = 1; y <= this.height; y++) {
		for (var x = 1; x <= this.width; x++) {
			if (x > 1 && x < this.width && y > 1 && y < this.height) continue;
			this.gotoxy(x, y);
			if (y == 1 && x == 1) {
				msg = ascii(218);
			} else if (y == 1 && x == this.width) {
				msg = ascii(191);
			} else if (y == this.height && x == 1) {
				msg = ascii(192);
			} else if (y == this.height && x == this.width) {
				msg = ascii(217);
			} else if (x == 1 || x == this.width) {
				msg = ascii(179);
			} else {
				msg = ascii(196);
			}
			if (sectionLength) {
				if (x == 1) {
					theColor = color[0];
				} else if (x % sectionLength == 0 && x < this.width) {
					theColor = color[x / sectionLength];
				} else if (x == this.width) {
					theColor = color[color.length - 1];
				}
			}
			this.putmsg(msg, theColor);
		}
	}
	this.popxy();
}

const root = js.exec_dir;

var settings,
	frame,
	browserFrame,
	statusBarFrame,
	areaFrame,
	speedFrame,
	pauseFrame;

const state = {
	status : bbs.sys_status,
	attr : console.attributes,
	syncTerm : false,
	fileList : [],
	pausing : false,
	drawfx : false,
	speed : 0,
	browser : null
};

const speedMap = [
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

function draw(file, cols, rows) {
	if ((typeof rows == 'undefined' || typeof cols == 'undefined')
        || (rows > console.screen_rows && cols <= console.screen_columns)
	) {
		return console.printfile(file, (state.pausing ? P_NONE : P_NOPAUSE) |P_CPM_EOF);
	}

	// Use graphic when the entire image fits on the screen
	var graphic = new Graphic(cols, rows);
	try {
		if (!graphic.load(file)) {
			alert("Graphic.load failure: " + file);
		} else {
            state.drawfx ? graphic.drawfx() : graphic.draw();
		}
	} catch(e) {
		// alert(e);
		console.printfile(file, (state.pausing ? P_NONE : P_NOPAUSE) |P_CPM_EOF);
	};
}

function drawFile(file) {

	const sauce = Sauce.read(file);
    const ext = (file_getext(file) || '').toLowerCase();
	const is_xbin = (ext == ".xb" || ext == ".xbin");

	if (ext == ".bin" || is_xbin) {
		if (!state.syncTerm) {
			return "Sorry, this file format is not supported by your terminal";
        }
		if (!is_xbin && (!sauce || sauce.datatype != Sauce.defs.datatype.bin)) {
			return "Sorry, this file has a missing or invalid SAUCE record";
		}
		var f = new File(file);
		if (!f.open("rb")) return "Failed to open: " + f.name;
		var image = {};
		if (!is_xbin) {
			image.width = sauce.cols;
			image.height = sauce.rows;
			image.flags = 0;
			if (sauce.ice_color) image.flags |= xbin.FLAG_NONBLINK;
			image.bin = f.read(image.width * image.height * 2);
		} else { /* XBin */
			image = xbin.read(f);
		}
		f.close();
		var warning = '';
		if (image.font) {
			if (!cterm.supports_fonts()) {
				warning += "Warning: your terminal does not support loadable fonts; font not used.\r\n";
			} else if (image.font.length && image.charheight != cterm.charheight(console.screen_rows)) {
				warning += format(
                    "Warning: file intended for a different char height (%u); font not used.\r\n",
                    image.charheight
                );
            }
		}
		if (image.palette && !cterm.supports_palettes()) {
			warning += "Warning: your terminal does not support loadable palettes; palette not used.\r\n";
		}
		if (warning.length) {
			alert(warning);
			console.pause();
		}
		cterm.xbin_draw(image);
		cterm.xbin_cleanup(image);
		image = undefined;
	} else if (state.syncTerm) {
		Ansi.send("speed", "set", state.speed);
		Ansi.send("ext_mode", "clear", "cursor");
		if (sauce.ice_color) Ansi.send("ext_mode", "set", "bg_bright_intensity");
		mswait(500);
		draw(file, sauce.cols, sauce.rows);
		console.getkey();
		Ansi.send("ext_mode", "clear", "bg_bright_intensity");
		Ansi.send("ext_mode", "set", "cursor");
		Ansi.send("speed", "clear");
	} else if (state.speed == 0) {
		draw(file, sauce.cols, sauce.rows);
		console.getkey();
	} else {	// TODO: terminate on Ctrl-Z char (CPM EOF) in the file
		var f = new File(file);
		if (!f.open("r")) return "Failed to open file: " + file.name;
		const contents = f.read().split("");
		f.close();
		const buf = Math.ceil((speedMap[state.speed] / 8) / 1000);
		if (!state.pausing) {
			bbs.sys_status&=(~SS_PAUSEON);
			bbs.sys_status|=SS_PAUSEOFF;
		}
		while (contents.length > 0) {
			console.print(contents.splice(0, buf).join(""));
			if (console.inkey(K_NONE, 1) != "") break;
		}
		console.getkey();
		bbs.sys_status|=SS_PAUSEON;
		bbs.sys_status&=(~SS_PAUSEOFF);
	}
	return true;
}

function printFile(file) {
	console.clear(BG_BLACK|LIGHTGRAY);
	frame.invalidate();
	const result = drawFile(file);
	console.clear(BG_BLACK|LIGHTGRAY);
	if (result !== true) {
		alert(result);
		console.pause();
	}
}

function showSpeed() {
	speedFrame.clear();
	speedFrame.putmsg(state.speed == 0 ? "Full" : speedMap[state.speed]);
}

function showPause() {
	pauseFrame.clear();
	pauseFrame.putmsg(state.pausing ? "On" : "Off");
}

function GalleryChooser() {

    var frame;
    var tree;
    var scrollBar;

	function getList() {
		const f = new File(root + "settings.ini");
		f.open("r");
		const galleries = f.iniGetAllObjects();
		f.close();
		return galleries;
	}

	this.open = function () {

		areaFrame.clear();
		areaFrame.putmsg(settings.top_level);

		frame = new Frame(
			browserFrame.x,
			browserFrame.y,
			browserFrame.width,
			browserFrame.height,
			browserFrame.attr,
			browserFrame
		);
		tree = new Tree(frame);
		tree.colors.fg = getColor(settings.fg);
		tree.colors.bg = getColor(settings.bg);
		tree.colors.lfg = getColor(settings.lfg);
		tree.colors.lbg = getColor(settings.lbg);

		getList().forEach(function (e) {
			e.colors = settings;
			tree.addItem(format("%-32s %s", e.name, e.description), function () {
				state.browser.close();
				state.browser = load(root + e.module, JSON.stringify(e));
				state.browser.open();
				areaFrame.clear();
				areaFrame.putmsg(e.name);
			});
		});

		scrollBar = new ScrollBar(tree);

		frame.open();
		tree.open();

	}

	this.close = function () {
		tree.close();
		scrollBar.close();
		frame.close();
	}

	this.cycle = function () {
		scrollBar.cycle();
	}

	this.getcmd = function (cmd) {
		tree.getcmd(cmd);
	}

}

function initDisplay() {

	console.clear(BG_BLACK|LIGHTGRAY);

	frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK|LIGHTGRAY);
    if (settings.header && settings.header_rows) {
        frame.height -= settings.header_rows;
        frame.y += settings.header_rows;
        const headerFrame = new Frame(1, 1, frame.width, settings.header_rows, BG_BLACK|LIGHTGRAY, frame);
    }

	browserFrame = new Frame(frame.x + 1, frame.y + 1, frame.width - 2, frame.height - 3, frame.attr, frame);
	statusBarFrame = new Frame(frame.x + 1, frame.y + frame.height - 2, frame.width - 2, 1, settings.sbg|settings.sfg, frame);
	statusBarFrame.putmsg(format("%-41s%-16s%-13s", "Area:","S)peed:","P)ause:"));
	statusBarFrame.gotoxy(statusBarFrame.width - 4, 1);
	statusBarFrame.putmsg("Q)uit");
	areaFrame = new Frame(statusBarFrame.x + 6, statusBarFrame.y, 35, 1, statusBarFrame.attr, statusBarFrame);
	speedFrame = new Frame(statusBarFrame.x + 49, statusBarFrame.y, 6, 1, statusBarFrame.attr, statusBarFrame);
	pauseFrame = new Frame(statusBarFrame.x + 65, statusBarFrame.y, 3, 1, statusBarFrame.attr, statusBarFrame);
    showSpeed();
	showPause();

	frame.open();

    frame.drawBorder(settings.border);
	frame.gotoxy(frame.width - 24, 1);
	frame.putmsg(ascii(180) + "\1h\1kansiview\1h\1w" + ascii(195));

    if (headerFrame) headerFrame.load(settings.header);

}

function initSettings() {
	const f = new File(root + "settings.ini");
	if(f.open("r")) {
		settings = f.iniGetObject();
		f.close();
	}
	if(!settings)
		settings = {};
	settings.fg = getColor(settings.fg);
	settings.bg = getColor(settings.bg);
	settings.lfg = getColor(settings.lfg);
	settings.lbg = getColor(settings.lbg);
	settings.sfg = getColor(settings.sfg);
	settings.sbg = getColor(settings.sbg);
	settings.border = settings.border.split(",");
	settings.border.forEach(function (e, i, a) { a[i] = getColor(e); });
    if (typeof settings.top_level != 'string') {
        settings.top_level = 'Gallery Menu';
    }
    if (settings.pause == true) state.pausing = true;
    if (typeof settings.speed == 'number') {
        var s = speedMap.indexOf(settings.speed);
        if (s > -1) state.speed = s;
    }
}

function init() {
	bbs.sys_status|=(SS_MOFF|SS_PAUSEON);
	bbs.sys_status&=(~SS_PAUSEOFF);
	state.syncTerm = cterm.query_da() !== false;
	initSettings();
	initDisplay();
	state.browser = new GalleryChooser();
	state.browser.open();
}

function cleanUp() {
	frame.close();
	bbs.sys_status = state.status;
	console.attributes = state.attr;
	console.clear();
	exit(0);
}

function handleInput(userInput) {
	switch (userInput) {
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
			if (state.browser instanceof GalleryChooser) {
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

function main() {
	while (!js.terminated) {
		handleInput(console.inkey(K_NONE, 5).toUpperCase());
		state.browser.cycle();
		if (frame.cycle()) {
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
	}
}

init();
main();
cleanUp();
