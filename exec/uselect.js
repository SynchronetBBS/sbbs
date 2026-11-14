// Replaces built-in behavior of console.uselect()
// for Synchronet v3.22+
// Set SCFG->System->Loadable Modules->Select Item to "uselect"

// Customizable via modopts/uselect options:
// - header_fmt
// - default_item_fmt
// - nondefault_item_fmt
// - prompt_fmt
// - more_fmt (shown when the items don't fit on one screen)

require("select_item.js", "select_item");

var options = load("modopts.js", "uselect:" + console.uselect_title);
if (!options)
	options = load("modopts.js", "uselect", {});

var dflt = Number(argv[0]);

function digits(n)
{
	if (n < 10)
		return 1;
	return 1 + digits(Math.floor(n / 10));
}

var items = console.uselect_items;
var widest = 0;
var current = -1;
for(var i = 0; i < items.length; ++i) {
	widest = Math.max(items[i].name.length, widest);
	if (current < 0 && items[i].num == dflt)
		current = i;
}

function print_item(i, is_default, redraw)
{
	var hotspot = i + 1;
	var fmt;
	if (is_default)
		fmt = options.default_item_fmt || "\x01U*\x01v%*s(\x01V%d\x01v) \x01U%s";
	else
		fmt = options.nondefault_item_fmt || "\x01U \x01v%*s(\x01V%d\x01v) \x01U%s";
	console.print(format(fmt
		, digits(hotspot) - digits(items.length), ""
		, hotspot, format("%-*s", widest, items[i].name)));
	if(digits(hotspot) < digits(items.length))
		hotspot += '\r';
	if (!redraw)
		console.add_hotspot(hotspot);
}

// Indent of the "more" indicator: aligned with the item numbers that follow
// the marker (e.g. '*') at the start of the item formats
function more_indent()
{
	var width = 1;
	var fmts = [options.default_item_fmt, options.nondefault_item_fmt];
	for (var i = 0; i < fmts.length; ++i) {
		if (fmts[i])
			width = Math.max(width, strip_ctrl(fmts[i].substr(0, fmts[i].indexOf('%'))).length);
	}
	return width + 1;
}

var choice = select_item({
	count: items.length,
	current: current,
	header: format(options.header_fmt || bbs.text(bbs.text.SelectItemHdr), console.uselect_title),
	prompt: function(num) {
		return format(options.prompt_fmt || bbs.text(bbs.text.SelectItemWhich), num);
	},
	print_item: print_item,
	more_fmt: options.more_fmt,
	more_indent: more_indent()
});
if (choice >= 0)
	exit(items[choice].num);
if (choice == -2) // Enter with no default item
	exit(dflt);
exit(-1);
