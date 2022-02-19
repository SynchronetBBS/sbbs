load('sbbsdefs.js');
load('frame.js');
load('tree.js');
load('scrollbar.js');
load('funclib.js');
require("mouse_getkey.js", "mouse_getkey");
const ansiterm = load({}, 'ansiterm_lib.js');
const lib = load({}, js.exec_dir + 'bullshit-lib.js');

js.branch_limit = 0;
js.time_limit = 0;

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

function mouse_enable(enable) {
	if (!console.term_supports(USER_ANSI)) return;
	ansiterm.send('mouse', enable ? 'set' : 'clear', 'normal_tracking');
}

function clicked_quit(i, f) {
	if (i.mouse && i.mouse.press && i.mouse.button == 0 && i.mouse.y == f.y + 1 && i.mouse.x > f.x + 1 && i.mouse.x < f.x + 11) {
		return true;
	}
	return false;
}

function Viewer(item, disp, settings) {

	const frames = {
		top: null,
		title: null,
		content: null
	};

	frames.top = new Frame(
		disp.frame.x,
		disp.frame.y + 3,
		disp.frame.width,
		disp.frame.height - 6,
		WHITE,
		disp.frame
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
    frames.content.atcodes = true;

	if (typeof item === 'number') {

		const msgBase = new MsgBase(settings.messageBase);
		msgBase.open();
		const header = msgBase.get_msg_header(item);
		const body = msgBase.get_msg_body(item);
		msgBase.close();

		frames.title.putmsg(format(
			'%-' + (disp.frame.width - 29) + 's%s',
			header.subject.substr(0, disp.frame.width - 30),
			system.timestr(header.when_written_time)
		), settings.colors.title);
		frames.content.putmsg(word_wrap(body, frames.content.width - 1));

	} else {

		frames.title.putmsg(format(
			'%-' + (disp.frame.width - 29) + 's%s',
			item.substr(0, disp.frame.width - 30),
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

		if (cmd.mouse && cmd.mouse.press) {
			if (clicked_quit(cmd, disp.footerFrame)) return this.getcmd({ key: 'Q' });
			if (cmd.mouse.button == 64) return this.getcmd({ key: KEY_UP });
			if (cmd.mouse.button == 65) return this.getcmd({ key: KEY_DOWN });
		}

		var ret = true;
		switch (cmd.key.toUpperCase()) {
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

function displayList(list, tree) {
    list.forEach(function (e) {
        tree.addItem(e.str, e.key);
    });
    tree.open();
}

function initDisplay(settings, list) {

	const frame = new Frame(1, 1, console.screen_columns, console.screen_rows, WHITE);

	const titleFrame = new Frame(frame.x, frame.y, frame.width, 3, settings.colors.title|settings.colors.titleBackground, frame);
	const footerFrame = new Frame(frame.x, frame.y + frame.height - 3, frame.width, 3, WHITE, frame);
	const treeFrame = new Frame(frame.x, titleFrame.y + titleFrame.height, frame.width, frame.height - titleFrame.height - footerFrame.height, WHITE, frame);
	const treeSubFrame = new Frame(treeFrame.x + 1, treeFrame.y + 2, treeFrame.width - 2, treeFrame.height - 3, WHITE, treeFrame);

	titleFrame.drawBorder(settings.colors.border);
	treeFrame.drawBorder(settings.colors.border);
	footerFrame.drawBorder(settings.colors.border);

	titleFrame.gotoxy(3, 2);
	titleFrame.putmsg('Bulletins');
	// titleFrame.gotoxy(frame.width - 25, 2);
	// titleFrame.putmsg('bullshit v3 by echicken', settings.colors.heading);

	treeFrame.gotoxy(3, 2);
	treeFrame.putmsg('Title', settings.colors.heading);
	treeFrame.gotoxy(treeFrame.x + treeFrame.width - 27, 2);
	treeFrame.putmsg('Date', settings.colors.heading);

	footerFrame.gotoxy(3, 2);
	footerFrame.putmsg('Q to quit, Up/Down arrows to scroll', settings.colors.footer);

	const tree = new Tree(treeSubFrame);
	tree.colors.lfg = settings.colors.lightbarForeground;
	tree.colors.lbg = settings.colors.lightbarBackground;
	tree.colors.fg = settings.colors.listForeground;

	const treeScroll = new ScrollBar(tree, { autohide: true });

	console.clear(LIGHTGRAY);
	frame.open();

	return {
		frame: frame,
		titleFrame: titleFrame,
		treeFrame: treeFrame,
		treeSubFrame: treeSubFrame,
		footerFrame: footerFrame,
		tree: tree,
		treeScroll: treeScroll,
	};

}

function init() {
	js.on_exit('mouse_enable(false);');
	js.on_exit('bbs.sys_status = ' + bbs.sys_status + ';');
	js.on_exit('console.attributes = ' + console.attributes + ';');
	js.on_exit('console.clear();');
	js.on_exit('console.clearkeybuffer();');
	bbs.sys_status|=SS_MOFF;
	mouse_enable(true);
}

function main() {
	var settings = lib.loadSettings(argv[0]);
	
	// if you set newOnly to logon, then on login time, it will treat it as newOnly=true and
	// only show if new bulletins, but at all other times, treat it as newOnly=false
	// (so always display them when called from external program menu context)
	if ((settings.newOnly == "logon") && (bbs.node_action != NODE_LOGN)) {
		settings.newOnly = false;
	}
	
	const list = lib.loadList(settings);
	
	if (!list.length && settings.newOnly) {
		return;
	}
	
	const disp = initDisplay(settings, list);
	displayList(list, disp.tree);
	var ret;
	var viewer;
	var userInput;
	while (!js.terminated) {
		userInput = mouse_getkey(K_NONE, 5, true);
		if (typeof viewer !== 'undefined') {
			ret = viewer.getcmd(userInput);
			viewer.cycle();
			if (!ret) viewer = undefined;
		} else {
			if (clicked_quit(userInput, disp.footerFrame) || userInput.key.toUpperCase() === 'Q' || userInput.key == '\x1B') {
				break;
			} else {
				ret = disp.tree.getcmd(userInput);
				disp.treeScroll.cycle();
				if (typeof ret == 'number' || typeof ret == 'string') {
                    lib.setBulletinRead(ret);
					viewer = new Viewer(ret, disp, settings);
				}
		    }
		}
		if (disp.frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
	}
	disp.frame.close();
}

try {
	init();
	main();
} catch (err) {
	log(LOG_ERR, JSON.stringify(err));
}
