// Replaces built-in behavior of console.uselect()
// for Synchronet v3.21+
// Set SCFG->System->Loadable Modules->Select Item to "uselect_tree"

// Customizable via modopts/uselect_tree options:
// - header_fmt
// - hex_hotkey_threshold = 15
// - alpha_hotkey_threshold = 26
// - hex_item_fmt
// - alpha_item_fmt
// - item_fmt
// - attr_fg
// - attr_bg
// - attr_lfg
// - attr_lbg
// - attr_kfg

require("sbbsdefs.js", "SS_MOFF");
load('frame.js');
load('tree.js');

var dflt = Number(argv[0]);

var options = load("modopts.js", "uselect_tree:" + console.uselect_title);
if (!options)
	options = load("modopts.js", "uselect_tree", {});

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

tree.colors.fg = eval(options.attr_fg || "LIGHTGRAY");
tree.colors.bg = eval(options.attr_bg || "BG_BLACK");
tree.colors.lfg = eval(options.attr_lfg || "WHITE");
tree.colors.lbg = eval(options.attr_lbg || "BG_CYAN");
tree.colors.kfg = eval(options.attr_kfg || "LIGHTCYAN");

for(var i = 0; i < items.length; ++i ) {
	if (items.length <= options.hex_hotkey_threshold || 15)
		tree.addItem(format(options.hex_item_fmt || "|%X %s", i + 1, items[i].name), items[i].num);
	else if (items.length <= options.alpha_hotkey_threshold || 26)
		tree.addItem(format(options.alpha_item_fmt || "|%c %s", ascii('A') + i, items[i].name), items[i].num);
	else
		tree.addItem(format(options.item_fmt || "|%s", items[i].name), items[i].num);
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