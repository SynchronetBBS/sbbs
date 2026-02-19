// Replaces built-in behavior of console.uselect()
// for Synchronet v3.21+
// Set SCFG->System->Loadable Modules->Select Item to "uselect_tree"

// Customizable via modopts/uselect_tree options:
// - header_fmt

require("sbbsdefs.js", "SS_MOFF");
load('frame.js');
load('tree.js');

var dflt = Number(argv[0]);

var options = load("modopts.js", "uselect");
if (!options)
	options = {};

require("mouse_getkey.js", "mouse_getkey");

js.on_exit('console.attributes = ' + console.attributes);
js.on_exit('bbs.sys_status = ' + bbs.sys_status);

bbs.sys_status |= SS_MOFF;

var header = format(options.header_fmt || bbs.text(bbs.text.SelectItemHdr), console.uselect_title);
var width = console.strlen(header) + 2;
var items = console.uselect_items;

for(var i = 0; i < items.length; ++i )
	width = Math.max(items[i].name.length + 6, width);

const frame = new Frame(1, 1, width, Math.min(console.screen_rows, items.length + 4), BG_BLUE|WHITE);
const main_frame = new Frame(1, 2, frame.width, frame.height - 1, BG_BLUE|LIGHTGRAY, frame);
const tree_frame = new Frame(2, main_frame.y + 1, main_frame.width - 2, main_frame.height - 2, BG_BLACK|LIGHTGRAY, main_frame);
const tree = new Tree(tree_frame);

frame.putmsg(header);

tree.colors.fg = LIGHTGRAY;
tree.colors.bg = BG_BLACK;
tree.colors.lfg = WHITE;
tree.colors.lbg = BG_CYAN;
tree.colors.kfg = LIGHTCYAN;

for(var i = 0; i < items.length; ++i ) {
	if (items.length <= 15)
		tree.addItem(format("|%X %s", i + 1, items[i].name), items[i].num);
	else if (items.length <= 26)
		tree.addItem(format("|%c %s", ascii('A') + i, items[i].name), items[i].num);
	else
		tree.addItem("|" + items[i].name, items[i].num);
	if (items[i].num == dflt)
		tree.index = i;
}

console.clear(BG_BLACK|LIGHTGRAY);
frame.open();
tree.open();
frame.cycle();

var retval = -1;
var key;
while (!js.terminated && bbs.online) {
    key = mouse_getkey(K_NOSPIN, undefined, false);
    if (key.key.toUpperCase() == console.quit_key)
		break;
    key = tree.getcmd(key);
	if (typeof key == 'number') {
		retval = key;
		break;
	}
    if (frame.cycle()) console.gotoxy(console.screen_columns, console.screen_rows);
}

frame.close();
exit(retval);