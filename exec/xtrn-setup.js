// $Id: xtrn-setup.js,v 1.10 2020/05/09 03:37:35 echicken Exp $
// vi: tabstop=4

// Locate/copy 3rd party door installer files
{
	var lib = load({}, 'install-3rdp-xtrn.js');
	var out = lib.scan();
	for(var i in out) {
		alert(out[i]);
	}
}

load('sbbsdefs.js');
load('frame.js');
load('tree.js');

require("mouse_getkey.js", "mouse_getkey");
const ansiterm = load({}, 'ansiterm_lib.js');

function mouse_enable(enable) {
	if (!console.term_supports(USER_ANSI)) return;
	ansiterm.send('mouse', enable ? 'set' : 'clear', 'normal_tracking');
}

js.on_exit('console.attributes = ' + console.attributes);
js.on_exit('bbs.sys_status = ' + bbs.sys_status);
js.on_exit('mouse_enable(false);');

bbs.sys_status|=SS_MOFF;
mouse_enable(true);

const frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLUE|WHITE);
const main_frame = new Frame(1, 2, frame.width, frame.height - 2, BG_BLACK|LIGHTGRAY, frame);
const tree_frame = new Frame(1, main_frame.y + 1, Math.floor(main_frame.width / 2), main_frame.height - 2, BG_BLACK|LIGHTGRAY, main_frame);
const info_frame = new Frame(tree_frame.width + 1, main_frame.y + 1, main_frame.width - tree_frame.width - 1, main_frame.height - 2, BG_BLACK|WHITE, main_frame);
info_frame.word_wrap = true;
const tree = new Tree(tree_frame);

frame.putmsg('External Program Setup');
frame.gotoxy(1, frame.height);
frame.putmsg('[Up/Down/Home/End] to navigate, [Enter] to select, [Q] to quit');

tree.colors.fg = LIGHTGRAY;
tree.colors.bg = BG_BLACK;
tree.colors.lfg = WHITE;
tree.colors.lbg = BG_CYAN;
tree.colors.kfg = LIGHTCYAN;

function find_startup_dir(dir)
{
	for (var i in xtrn_area.prog) {
		if (!xtrn_area.prog[i].startup_dir)
			continue;
		var path = backslash(fullpath(xtrn_area.prog[i].startup_dir));
		if (path == dir)
			return i;
	}
	return false;
}

var longest = 0;
directory(system.exec_dir + '../xtrn/*', GLOB_ONLYDIR).forEach(function (e) {
	var dir = backslash(fullpath(e));
	if (find_startup_dir(dir) !== false)
		return;
    const ini = e + '/install-xtrn.ini';
    if (!file_exists(ini)) return;
    const f = new File(ini);
    if (!f.open('r')) {
		alert("Error " + f.error + " opening " + f.name);
		return;
	}
    const xtrn = f.iniGetObject();
    f.close();
	if(xtrn['xtrn-setup'] === false)
		return;
	if(!xtrn.Name) {
		alert("Skipping file with no 'Name' value: " + f.name);
		return;
	}
    const item = tree.addItem(xtrn.Name, function () {
		console.clear(LIGHTGRAY);
        js.exec('install-xtrn.js', {}, e);
		console.pause();
		frame.invalidate();
    });
    item.__xtrn_setup = xtrn;
    if (xtrn.Name.length > longest) longest = xtrn.Name.length;
});

if (!tree.items.length) {
	alert("No installable external programs found");
	exit(0);
}

tree_frame.width = longest + 1;
info_frame.x = tree_frame.width + 2;
info_frame.width = main_frame.width - tree_frame.width - 2;

console.clear(BG_BLACK|LIGHTGRAY);
frame.open();
tree.open();
frame.cycle();

var t;
var key;
var xtrn;
console.ungetstr(KEY_UP);
while (!js.terminated) {
    key = mouse_getkey(K_NOSPIN, undefined, true);//console.getkey();
    if (key.key.toLowerCase() == 'q') break;
    if (key.mouse && key.mouse.press && key.mouse.button == 0 && key.mouse.y == frame.y + frame.height - 1 && key.mouse.x >= 52 && key.mouse.x <= 65) break;
    t = tree.getcmd(key);
    if ((key.mouse && t) || key.key == KEY_UP || key.key == KEY_DOWN || key.key == KEY_HOME || key.key == KEY_END) {
        xtrn = tree.currentItem.__xtrn_setup;
        info_frame.erase(' ');
        info_frame.putmsg('\x01h\x01w' + xtrn.Name + '\r\n');
        if (xtrn.Desc) {
			info_frame.putmsg('\x01n\x01w' + xtrn.Desc + '\r\n');
		}
		info_frame.crlf();
        if (xtrn.By) {
			info_frame.putmsg('\x01h\x01cBy\x01w:\r\n');
			info_frame.putmsg('\x01w' + xtrn.By + '\r\n\r\n');
		}
        if (xtrn.Cats) {
			info_frame.putmsg('\x01h\x01cCategories\x01w:\r\n');
			info_frame.putmsg('\x01n' + xtrn.Cats + '\r\n\r\n');
		}
        if (xtrn.Subs) {
			info_frame.putmsg('\x01h\x01cSubcategories\x01w:\r\n');
			info_frame.putmsg('\x01n' + xtrn.Subs + '\r\n');
		}
    }
    if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
}

frame.close();
