// Replaces built-in behavior of console.uselect()
// for Synchronet v3.21+
// Set SCFG->System->Loadable Modules->Select Item to "uselect"

// Customizable via modopts/uselect options:
// - header_fmt
// - default_item_fmt
// - nondefault_item_fmt
// - prompt_fmt

var options = load("modopts.js", "uselect:" + console.uselect_title);
if (!options)
	options = load("modopts.js", "uselect", {});

var dflt = Number(argv[0]);

function digits(n)
{
    if (n/10 == 0)
        return 1;
    return 1 + digits(Math.floor(n / 10));
}
console.print(format(options.header_fmt || bbs.text(bbs.text.SelectItemHdr), console.uselect_title));

var items = console.uselect_items;
var udflt = 1;
for(var i = 0; i < items.length; ++i ) {
	var hotspot = i + 1;
	var fmt = options.default_item_fmt || "\x01U*\x01v%*s(\x01V%d\x01v) \x01U%s";
	if (items[i].num == dflt)
		udflt = hotspot;
	else
		fmt = options.nondefault_item_fmt || "\x01U \x01v%*s(\x01V%d\x01v) \x01U%s";
	console.print(format(fmt
		, digits(hotspot) - digits(items.length), ""
		, hotspot, items[i].name));
	if(digits(hotspot) < digits(items.length))
		hotspot += '\r';
	console.add_hotspot(hotspot);
	console.newline();
}

console.mnemonics(format(options.prompt_fmt || bbs.text(bbs.text.SelectItemWhich), udflt));
var choice = console.getnum(items.length);
if (choice > 0)
	choice = items[choice - 1].num;
else if (choice == 0)
	choice = dflt;
exit(choice);
