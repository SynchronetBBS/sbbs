load('sbbsdefs.js');
load('frame.js');
load('tree.js');

const con_attr = console.attributes;
const sys_status = bbs.sys_status;
bbs.sys_status|=SS_MOFF;

console.clear(BG_BLACK|LIGHTGRAY);

const frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLUE|WHITE);
const main_frame = new Frame(1, 2, frame.width, frame.height - 2, BG_BLACK|LIGHTGRAY, frame);
const tree_frame = new Frame(1, main_frame.y + 1, Math.floor(main_frame.width / 2), main_frame.height - 2, BG_BLACK|LIGHTGRAY, main_frame);
const info_frame = new Frame(tree_frame.width + 1, main_frame.y + 1, main_frame.width - tree_frame.width - 1, main_frame.height - 2, BG_BLACK|WHITE, main_frame);
const tree = new Tree(tree_frame);

frame.putmsg('FTN Setup');
frame.gotoxy(1, frame.height);
frame.putmsg('[UP/DN] to navigate, [Enter] to select, [Q] to quit');

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
        const item = tree.addItem(format('Zone %3d: %s', zone, net.name), function () {
            console.clear(BG_BLACK|LIGHTGRAY);
            bbs.exec('?init-fidonet ' + zone);
            frame.invalidate();
        });
        if (item.text.length > longest) longest = item.text.length;
        item.__ftn_setup = net;
    });
	f.close();
}

tree_frame.width = longest + 1;
info_frame.x = tree_frame.width + 2;
info_frame.width = main_frame.width - tree_frame.width - 2;

frame.open();
tree.open();
frame.cycle();

var key;
var zone;
console.ungetstr(KEY_UP);
while (!js.terminated) {
    key = console.getkey();
    if (key == 'q') break;
    tree.getcmd(key);
    if (key == KEY_UP || key == KEY_DOWN) {
        zone = tree.currentItem.__ftn_setup;
        info_frame.clear();
        info_frame.putmsg('\1h\1w' + zone.name + '\r\n');
        if (zone.desc) info_frame.putmsg('\1n\1w' + word_wrap(zone.desc, info_frame.width) + '\r\n');
        if (zone.info) info_frame.putmsg('\1h\1cInformation\1w: \1n' + zone.info + '\r\n');
        if (zone.coord) info_frame.putmsg('\1h\1cCoordinator\1w: \1n' + zone.coord + '\r\n');
        if (zone.email) info_frame.putmsg('      \1h\1cEmail\1w: \1n' + zone.email + '\r\n');
    }
    if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
}

frame.close();

console.attributes = con_attr;
bbs.sys_status = sys_status;