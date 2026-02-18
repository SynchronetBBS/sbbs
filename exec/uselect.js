// Replaces built-in behavior of console.uselect()
// for Synchronet v3.21+
// Set SCFG->System->Loadable Modules->Select Item to "uselect"

var dflt = Number(argv[0]);

function digits(n)
{
    if (n/10 == 0)
        return 1;
    return 1 + digits(Math.floor(n / 10));
}
console.print(format(bbs.text(bbs.text.SelectItemHdr), console.uselect_title));

var items = console.uselect_items;
var udflt = 1;
for(var i = 0; i < items.length; ++i ) {
	var ch = '*';
	if (items[i].num == dflt)
		udflt = i + 1;
	else
		ch = ' ';
	var hotspot = i + 1;
	console.print(format("\x01U%c\x01v%*s(\x01V%d\x01v) \x01U%s"
		, ch
		, digits(hotspot) - digits(items.length), ""
		, hotspot, items[i].name));
	if(digits(hotspot) < digits(items.length))
		hotspot += '\r';
	console.add_hotspot(hotspot);
	console.newline();
}

console.mnemonics(format(bbs.text(bbs.text.SelectItemWhich), udflt));
var choice = console.getnum(items.length);
if (choice > 0)
	choice = items[choice - 1].num;
else
	choice = dflt;
exit(choice);
