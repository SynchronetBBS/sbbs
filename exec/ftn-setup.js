// $Id: ftn-setup.js,v 1.13 2020/05/09 03:37:35 echicken Exp $

load('sbbsdefs.js');
load('frame.js');
load('tree.js');
const fidoaddr = load({}, 'fidoaddr.js');
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

const addrs = {};
system.fido_addr_list.forEach(function (e) {
    const a = fidoaddr.parse(e);
    addrs[a.zone] = e;
});

const frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLUE|WHITE);
const main_frame = new Frame(1, 2, frame.width, frame.height - 2, BG_BLACK|LIGHTGRAY, frame);
const tree_frame = new Frame(1, main_frame.y + 1, Math.floor(main_frame.width / 2), main_frame.height - 2, BG_BLACK|LIGHTGRAY, main_frame);
const info_frame = new Frame(tree_frame.width + 1, main_frame.y + 1, main_frame.width - tree_frame.width - 1, main_frame.height - 2, BG_BLACK|WHITE, main_frame);
info_frame.word_wrap = true;
const tree = new Tree(tree_frame);

frame.putmsg('FTN Setup');
frame.gotoxy(1, frame.height);
frame.putmsg('[Up/Down/Home/End] to navigate, [Enter] to select, [Q] to quit');

tree.colors.fg = LIGHTGRAY;
tree.colors.bg = BG_BLACK;
tree.colors.lfg = WHITE;
tree.colors.lbg = BG_CYAN;
tree.colors.kfg = LIGHTCYAN;

var longest = 0;
var f = new File(system.exec_dir + 'init-fidonet.ini');
if (f.open('r')) {
    f.iniGetSections('zone:', 'zone').forEach(function (e) {
        const net = f.iniGetObject(e);
        const zone = e.substr(5);
        const item = tree.addItem(format('Zone %5d: %s', zone, net.name), function () {
            console.clear(BG_BLACK|LIGHTGRAY);
            js.exec('init-fidonet.js', {}, zone);
            frame.invalidate();
        });
        if (item.text.length > longest) longest = item.text.length;
        item.__ftn_setup = net;
        item.__ftn_setup._zone_number = parseInt(zone, 10);
    });
	f.close();
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
var zone;
console.ungetstr(KEY_UP);
while (!js.terminated) {
    key = mouse_getkey(K_NOSPIN, undefined, true);
    //key = console.getkey();
    if (key.key.toLowerCase() == 'q') break;
	if (key.mouse && key.mouse.press && key.mouse.button == 0 && key.mouse.y == frame.y + frame.height - 1 && key.mouse.x >= 52 && key.mouse.x <= 65) break;
    t = tree.getcmd(key);
    if ((key.mouse && t) || key.key == KEY_UP || key.key == KEY_DOWN || key.key == KEY_HOME || key.key == KEY_END) {
        zone = tree.currentItem.__ftn_setup;
        info_frame.erase(' ');
        info_frame.putmsg('\1h\1w' + zone.name + '\r\n');
        if (zone.desc) {
            info_frame.putmsg('\1n\1w' + zone.desc + '\r\n\r\n');
        }
        if (zone.info) {
            info_frame.putmsg('\1h\1cInformation\1w:\r\n');
            info_frame.putmsg('\1n' + zone.info + '\r\n\r\n');
        }
        if (zone.coord) {
            info_frame.putmsg('\1h\1cCoordinator\1w:\r\n');
            info_frame.putmsg('\1n' + zone.coord + '\r\n\r\n');
        }
        if (zone.email) {
            info_frame.putmsg('\1h\1cEmail\1w:\r\n');
            info_frame.putmsg('\1n' + zone.email + '\r\n\r\n');
        }
        if (addrs[zone._zone_number]) {
            info_frame.putmsg('\1h\1rExisting address found: ' + addrs[zone._zone_number] + '\r\n');
        }
    }
    if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
}

frame.close();
console.creturn();
console.cleartoeol();
