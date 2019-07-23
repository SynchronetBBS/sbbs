load('sbbsdefs.js');
load('frame.js');
load('tree.js');
load('scrollbar.js');
load('funclib.js');
const lib = load({}, js.exec_dir + 'bullshit-lib.js');

js.branch_limit = 0;
js.time_limit = 0;

var settings, frame, tree, treeScroll, viewer, list;

Frame.prototype.drawBorder = function (color, title) {
	this.pushxy();
	var theColor = color;
	if (Array.isArray(color)) {
		var sectionLength = Math.round(this.width / color.length);
	}
	for (var y = 1; y <= this.height; y++) {
		for (var x = 1; x <= this.width; x++) {
			if (x > 1 && x < this.width && y > 1 && y < this.height) continue;
			var msg;
			this.gotoxy(x, y);
			if (y === 1 && x === 1) {
				msg = ascii(218);
			} else if (y === 1 && x === this.width) {
				msg = ascii(191);
			} else if (y === this.height && x === 1) {
				msg = ascii(192);
			} else if (y === this.height && x === this.width) {
				msg = ascii(217);
			} else if (x === 1 || x === this.width) {
				msg = ascii(179);
			} else {
				msg = ascii(196);
			}
			if (Array.isArray(color)) {
				if (x === 1) {
					theColor = color[0];
				} else if (x % sectionLength === 0 && x < this.width) {
					theColor = color[x / sectionLength];
				} else if (x === this.width) {
					theColor = color[color.length - 1];
				}
			}
			this.putmsg(msg, theColor);
		}
	}
	if (typeof title === 'object') {
		this.gotoxy(title.x, title.y);
		this.attr = title.attr;
		this.putmsg(ascii(180) + title.text + ascii(195));
	}
	this.popxy();
}

function Viewer(item) {

	const frames = {
		top: null,
		title: null,
		content: null
	};

	frames.top = new Frame(
		frame.x,
		frame.y + 3,
		frame.width,
		frame.height - 6,
		WHITE,
		frame
	);
	frames.top.drawBorder(settings.colors.border);

	frames.title = new Frame(
		frames.top.x + 1,
		frames.top.y + 1,
		frames.top.width - 2,
		1,
		settings.colors.title,
		frames.top
	);

	frames.content = new Frame(
		frames.top.x + 1,
		frames.title.y + frames.title.height + 1,
		frames.top.width - 2,
		frames.top.height - frames.title.height - 3,
		settings.colors.text,
		frames.top
	);

	if (typeof item === 'number') {

		const msgBase = new MsgBase(settings.messageBase);
		msgBase.open();
		const header = msgBase.get_msg_header(item);
		const body = msgBase.get_msg_body(item);
		msgBase.close();

		frames.title.putmsg(format(
			'%-' + (frame.width - 29) + 's%s',
			header.subject.substr(0, frame.width - 30),
			system.timestr(header.when_written_time)
		), settings.colors.title);
		frames.content.putmsg(word_wrap(body, frames.content.width - 1));

	} else {

		frames.title.putmsg(format(
			'%-' + (frame.width - 29) + 's%s',
			item.substr(0, frame.width - 30),
			system.timestr(file_date(settings.files[item]))
		), settings.colors.title);

		frames.content.x = frames.top.x;
		frames.content.width = frames.top.width;

		try {
			frames.content.load(settings.files[item]);
		} catch (err) {
			log(LOG_ERR, err);
		}

		frames.content.width = frames.top.width - 2;
		frames.content.x = frames.top.x + 1;

	}

	frames.content.scrollTo(0, 0);
	frames.content.h_scroll = true;

	const scrollbar = new ScrollBar(frames.content, { autohide: true });
	frames.top.open();

	this.getcmd = function (cmd) {

		var ret = true;
		switch (cmd.toUpperCase()) {
			case '\x1B':
			case 'Q':
				ret = false;
				frames.top.close();
				break;
			case KEY_UP:
				if (frames.content.data_height > frames.content.height &&
					frames.content.offset.y >= 1
				) {
					frames.content.scroll(0, -1);
				}
				break;
			case KEY_DOWN:
				if (frames.content.data_height > frames.content.height &&
					frames.content.data_height - frames.content.offset.y >
						frames.content.height
				) {
					frames.content.scroll(0, 1);
				}
				break;
			case KEY_LEFT:
				if (frames.content.data_width > frames.content.width &&
					frames.content.offset.x >= 1
				) {
					frames.content.scroll(-1, 0);
				}
				break;
			case KEY_RIGHT:
				if (frames.content.data_width > frames.content.width &&
					frames.content.data_width - frames.content.offset.x >
						frames.content.width
				) {
					frames.content.scroll(1, 0);
				}
				break;
			default:
				break;
		}

		return ret;

	}

	this.cycle = function () {
		scrollbar.cycle();
	}

}

function displayList() {
    list.forEach(function (e) {
        tree.addItem(e.str, e.key);
    });
    tree.open();
}

function initDisplay() {

	frame = new Frame(
		1,
		1,
		console.screen_columns,
		console.screen_rows,
		WHITE
	);

	var titleFrame = new Frame(
		frame.x,
		frame.y,
		frame.width,
		3,
		WHITE,
		frame
	);

	var footerFrame = new Frame(
		frame.x,
		frame.y + frame.height - 3,
		frame.width,
		3,
		WHITE,
		frame
	);

	var treeFrame = new Frame(
		frame.x,
		titleFrame.y + titleFrame.height,
		frame.width,
		frame.height - titleFrame.height - footerFrame.height,
		WHITE,
		frame
	);

	var treeSubFrame = new Frame(
		treeFrame.x + 1,
		treeFrame.y + 2,
		treeFrame.width - 2,
		treeFrame.height - 3,
		WHITE,
		treeFrame
	);

	titleFrame.drawBorder(settings.colors.border);
	treeFrame.drawBorder(settings.colors.border);
	footerFrame.drawBorder(settings.colors.border);

	titleFrame.gotoxy(3, 2);
	titleFrame.putmsg('Bulletins', settings.colors.title);
	titleFrame.gotoxy(frame.width - 25, 2);
	titleFrame.putmsg('bullshit v3 by echicken', settings.colors.heading);

	treeFrame.gotoxy(3, 2);
	treeFrame.putmsg('Title', settings.colors.heading);
	treeFrame.gotoxy(treeFrame.x + treeFrame.width - 27, 2);
	treeFrame.putmsg('Date', settings.colors.heading);

	footerFrame.gotoxy(3, 2);
	footerFrame.putmsg(
		'Q to quit, Up/Down arrows to scroll', settings.colors.footer
	);

	tree = new Tree(treeSubFrame);
	tree.colors.lfg = settings.colors.lightbarForeground;
	tree.colors.lbg = settings.colors.lightbarBackground;
	tree.colors.fg = settings.colors.listForeground;

	treeScroll = new ScrollBar(tree, { autohide: true });

	console.clear(LIGHTGRAY);
	frame.open();

}

function init() {
	settings = lib.loadSettings();
    list = lib.loadList(settings);
}

function main() {
	if (!list.length && settings.newOnly) return;
    initDisplay();
    displayList();
	while (!js.terminated) {
		var userInput = console.inkey(K_NONE, 5);
		if (typeof viewer !== 'undefined') {
			var ret = viewer.getcmd(userInput);
			viewer.cycle();
			if (!ret) viewer = undefined;
		} else {
			if (userInput.toUpperCase() === 'Q' || userInput == '\x1B') {
				break;
			} else {
				var ret = tree.getcmd(userInput);
				treeScroll.cycle();
				if (typeof ret == 'number' || typeof ret == 'string') {
                    lib.setBulletinRead(ret);
					viewer = new Viewer(ret);
				}
		    }
		}
		if (frame.cycle()) {
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
	}
}

function cleanUp() {
	if (frame) frame.close();
}

try {
	init();
	main();
	cleanUp();
} catch (err) {
	log(LOG_ERR, err);
}
