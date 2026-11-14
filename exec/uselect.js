// Replaces built-in behavior of console.uselect()
// for Synchronet v3.21+
// Set SCFG->System->Loadable Modules->Select Item to "uselect"

// Customizable via modopts/uselect options:
// - header_fmt
// - default_item_fmt
// - nondefault_item_fmt
// - prompt_fmt
// - more_fmt (shown when the items don't fit on one screen)

require("sbbsdefs.js", "K_UPPER");

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

// Mirrors sbbs_t::getnum(), plus returns navigation keys to the caller
function getnum(max)
{
	var n = 0;
	var len = 0;

	while (bbs.online && !js.terminated) {
		var key = console.getkey(K_UPPER);
		if (console.aborted) {
			console.newline();
			console.line_counter = 0;
			return -1;
		}
		switch (key) {
			case KEY_UP:
			case KEY_DOWN:
			case KEY_HOME:
			case KEY_END:
			case KEY_PAGEUP:
			case KEY_PAGEDN:
				if (len == 0)
					return key;
				break;
			case console.quit_key:
				console.print(key);
				console.newline();
				console.line_counter = 0;
				return -1;
			case '\r':
				console.newline();
				console.line_counter = 0;
				return n;
			case '\b':
			case '\x7f':
				if (len > 0) {
					console.backspace();
					n = Math.floor(n / 10);
					len--;
				}
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				var digit = ascii(key) - ascii('0');
				if ((n * 10) + digit > max || (digit == 0 && len == 0))
					break;
				n *= 10;
				n += digit;
				len++;
				console.print(key);
				if (n * 10 > max && console.keyboard_buffer_level < 1) {
					console.newline();
					console.line_counter = 0;
					return n;
				}
				break;
		}
	}
	return -1;
}

var items = console.uselect_items;
var widest = 0;
for(var i = 0; i < items.length; ++i)
	widest = Math.max(items[i].name.length, widest);

function count_lines(str)
{
	return str.split('\n').length - 1;
}

var top = 0; // index of first item displayed (when paging)

while (bbs.online && !js.terminated) {
	var hdr = format(options.header_fmt || bbs.text(bbs.text.SelectItemHdr), console.uselect_title);
	var prompt = format(options.prompt_fmt || bbs.text(bbs.text.SelectItemWhich), 1);
	// Rows available for items: the screen, less the header, the prompt
	// (and the cursor line it ends on)
	var rows = console.screen_rows - count_lines(hdr) - count_lines(prompt) - 1;
	var page = items.length;
	// The "more" indicator goes on the prompt's leading blank line, if it has one
	var more_on_blank = /^\r?\n/.test(prompt);
	if (page > rows)
		page = Math.max(more_on_blank ? rows : rows - 1, 1);

	var udflt = 1;
	for(var i = 0; i < items.length; ++i) {
		if (items[i].num == dflt) {
			udflt = i + 1;
			break;
		}
	}
	// Scroll the page so the default item is visible
	if (udflt - 1 < top)
		top = udflt - 1;
	else if (udflt - 1 >= top + page)
		top = udflt - page;
	top = Math.max(0, Math.min(top, items.length - page));

	console.clear_hotspots();
	console.print(hdr);
	console.line_counter = 0;
	for(var i = top; i < items.length && i < top + page; ++i) {
		var hotspot = i + 1;
		var fmt = options.default_item_fmt || "\x01U*\x01v%*s(\x01V%d\x01v) \x01U%s";
		if (items[i].num != dflt)
			fmt = options.nondefault_item_fmt || "\x01U \x01v%*s(\x01V%d\x01v) \x01U%s";
		console.print(format(fmt
			, digits(hotspot) - digits(items.length), ""
			, hotspot, format("%-*s", widest, items[i].name)));
		if(digits(hotspot) < digits(items.length))
			hotspot += '\r';
		console.add_hotspot(hotspot);
		console.newline();
	}
	if (page < items.length) {
		console.print(format(options.more_fmt || "\x01n\x01h\x01k  [%u-%u of %u]"
			, top + 1, top + page, items.length));
		if (!more_on_blank)
			console.newline();
	}
	console.line_counter = 0;

	console.mnemonics(format(options.prompt_fmt || bbs.text(bbs.text.SelectItemWhich), udflt));
	var choice = getnum(items.length);
	if (typeof choice === 'string') {
		var cur = udflt - 1;
		if (choice == KEY_UP)
			cur--;
		else if (choice == KEY_DOWN)
			cur++;
		else if (choice == KEY_PAGEUP) {
			cur -= page;
			top -= page;
		} else if (choice == KEY_PAGEDN) {
			cur += page;
			top += page;
		} else if (choice == KEY_HOME)
			cur = 0;
		else if (choice == KEY_END)
			cur = items.length - 1;
		cur = Math.max(0, Math.min(cur, items.length - 1));
		dflt = items[cur].num;
		continue;
	}
	if (choice > 0)
		choice = items[choice - 1].num;
	else if (choice == 0)
		choice = dflt;
	exit(choice);
}
