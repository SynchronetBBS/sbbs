// Meme selection/chooser library
// @format.tab-size 4, @format.use-tabs true

// Supported options
//   color (default: 0)
//   border (default: 0)
//   random (default: false)
//   max_length (default: 500)
//   width (default: 39)
//   justify (default: center)

"use strict";
require("key_defs.js", "KEY_LEFT");

var lib = load({}, "meme_lib.js");

function choose(border)
{
	console.mnemonics(format("~Border, ~Color, ~Justify, ~@Quit@, or [Select]: "));
	var ch = console.getkeys("BCJ" + KEY_LEFT + KEY_RIGHT + "\r" + console.next_key + console.prev_key + console.quit_key, lib.BORDER_COUNT);
	if (typeof ch == "number")
		return ch - 1;
	switch (ch) {
		case console.quit_key:
			return false;
		case '\r':
			return true;
		case 'C':
		case 'J':
		case 'B':
			return ch;
		case KEY_UP:
		case KEY_LEFT:
		case console.prev_key:
			return console.prev_key;
		default:
			return console.next_key;
	}
}

function main(text, options)
{
	var attr = [
		"\x01H\x01W\x011",
		"\x01H\x01W\x012",
		"\x01H\x01W\x013",
		"\x01H\x01W\x014",
		"\x01H\x01W\x015",
		"\x01H\x01W\x016",
		"\x01N\x01K\x017",
	];
	if (!options) options = {};
	var justify = options.justify || 0;
	var border = options.border || 0;
	var color = options.color || 0;
	if (options.random) {
		border = random(lib.BORDER_COUNT);
		color = random(attr.length);
	}
	var msg;
	while (!js.terminated) {
		msg = lib.generate(options.width || 39, attr[color % attr.length], border  % lib.BORDER_COUNT, text, justify % lib.JUSTIFY_COUNT);
		console.clear();
		console.attributes = WHITE | HIGH;
		console.print(format("Meme \x01N\x01C(border \x01H%u \x01N\x01Cof \x01H%u\x01N\x01C, color \x01H%u\x01N\x01C of \x01H%u\x01N\x01C):"
			, (border % lib.BORDER_COUNT) + 1, lib.BORDER_COUNT
			, (color % attr.length) + 1, attr.length));
		console.newline(2);
		print(msg);
		var ch = choose(border);
		if (ch === false)
			return false;
		if (ch === true)
			return msg;
		if (typeof ch == "number")
			border = ch;
		else if (ch === 'C')
			++color;
		else if (ch === 'J')
			++justify;
		else if (ch === 'B')
			++border;
		else if (ch == console.next_key && border < lib.BORDER_COUNT - 1)
			++border;
		else if (ch == console.prev_key && border > 0)
			--border;
		else
			console.beep();
	}
	return null;
}

main.apply(null, argv);
