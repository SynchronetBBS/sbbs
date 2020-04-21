// $Id$

load('sbbsdefs.js');
load('frame.js');
load('tree.js');

js.on_exit('console.attributes = ' + console.attributes);
js.on_exit('bbs.sys_status = ' + bbs.sys_status);

bbs.sys_status|=SS_MOFF;

const frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLUE|WHITE);
const main_frame = new Frame(1, 2, frame.width, frame.height - 2, BG_BLACK|LIGHTGRAY, frame);
const tree_frame = new Frame(1, main_frame.y + 1, Math.floor(main_frame.width / 2), main_frame.height - 2, BG_BLACK|LIGHTGRAY, main_frame);
const info_frame = new Frame(tree_frame.width + 1, main_frame.y + 1, main_frame.width - tree_frame.width - 1, main_frame.height - 2, BG_BLACK|WHITE, main_frame);
const tree = new Tree(tree_frame);

frame.putmsg('External Program Setup');
frame.gotoxy(1, frame.height);
frame.putmsg('[Up/Down/Home/End] to navigate, [Enter] to select, [Q] to quit');

tree.colors.fg = LIGHTGRAY;
tree.colors.bg = BG_BLACK;
tree.colors.lfg = WHITE;
tree.colors.lbg = BG_CYAN;
tree.colors.kfg = LIGHTCYAN;

var longest = 0;
directory(system.exec_dir + '../xtrn/*', GLOB_ONLYDIR).filter(function (e) {
    const ini = e + '/install-xtrn.ini';
    if (!file_exists(ini)) return;
    const f = new File(ini);
    if (!f.open('r')) return;
    const xtrn = f.iniGetObject();
    f.close();
    const item = tree.addItem(xtrn.Name, function () {
        js.exec('install-xtrn', {}, e);
    });
    item.__xtrn_setup = xtrn;
    if (xtrn.Name.length > longest) longest = xtrn.Name.length;
});

tree_frame.width = longest + 1;
info_frame.x = tree_frame.width + 2;
info_frame.width = main_frame.width - tree_frame.width - 2;

console.clear(BG_BLACK|LIGHTGRAY);
frame.open();
tree.open();
frame.cycle();

var key;
var xtrn;
console.ungetstr(KEY_UP);
while (!js.terminated) {
    key = console.getkey();
    if (key.toLowerCase() == 'q') break;
    tree.getcmd(key);
    if (key == KEY_UP || key == KEY_DOWN || key == KEY_HOME || key == KEY_END) {
        xtrn = tree.currentItem.__xtrn_setup;
        info_frame.clear();
        info_frame.putmsg('\x01h\x01w' + xtrn.Name + '\r\n');
        if (xtrn.Desc) info_frame.putmsg('\x01n\x01w' + word_wrap(xtrn.Desc, info_frame.width) + '\r\n');
        if (xtrn.By) info_frame.putmsg('\x01h\x01cBy\x01w:\r\n\x01w' + xtrn.By + '\r\n\r\n');
        if (xtrn.Cats) info_frame.putmsg('\x01h\x01cCategories:\x01w:\r\n\x01n' + xtrn.Cats + '\r\n\r\n');
        if (xtrn.Subs) info_frame.putmsg('\x01h\x01cSubcategories:\x01w:\r\n\x01n' + xtrn.Subs + '\r\n\r\n');
    }
    if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
}

frame.close();
