// Lightbar navigation for display files
//
// A display file marks each selectable region with a pair of @-codes:
//
//     @BAR:M@ Multi-player @BAR@
//
// or, when the label is plain text, with the single-code shorthand (whose
// underscores are displayed as spaces):
//
//     @[_Multi-player_]M@
//
// and may name the selected-state color with @BARATTR:<attr>@ (or a bare
// @BARATTR@ to use the attribute in effect at that point of the file) and,
// with @BARATTR:<attr>:<key-attr>@, a foreground color for each item's
// command key. @BARDEFAULT:<cmd>@ names the item to highlight first.
// The Terminal Server collects those regions while it displays the file and
// exposes them as console.bars; this library adds the arrow-key navigation.
//
// Regions that share a command are one item, so a multi-row button is just
// the same @BAR:<cmd>@ on each of its rows.
//
// Usage:
//     var bar = new (load({}, "menubar.js").MenuBar)();
//     console.printfile(menu_file, P_NOPAUSE);
//     bar.refresh();                       // after every (re)display of the file
//     var key = bar.getkey("MSQ");         // blocking, or...
//     key = bar.handle(console.inkey(K_UPPER, 1000));   // ...one key at a time
//
// With no @BAR codes in the file, handle() passes every key through unchanged
// and getkey() behaves like a plain hot-key prompt.

require("sbbsdefs.js", "K_UPPER");
require("key_defs.js", "KEY_UP");

// 'cmd' is the item to highlight first (default: the one the file names with
// @BARDEFAULT:<cmd>@, else the first one in the file); pass false to highlight
// nothing until the user presses a navigation key
function MenuBar(cmd)
{
	this.items = [];
	this.current = -1;
	this.cmd = cmd;
	this.attr = 0;
	this.hot_attr = 0;
}

// Collect the regions of the file(s) just displayed and highlight the current
// item; returns the number of items found
MenuBar.prototype.refresh = function()
{
	var bars = console.bars || [];
	var i, j;

	this.items = [];
	for (i = 0; i < bars.length; i++) {
		for (j = 0; j < this.items.length; j++) {
			if (this.items[j].cmd == bars[i].cmd)
				break;
		}
		if (j == this.items.length)
			this.items.push({ cmd: bars[i].cmd, x: bars[i].x, y: bars[i].y, parts: [] });
		this.items[j].parts.push(bars[i]);
	}
	this.attr = console.bar_attr || 0;
	this.hot_attr = console.bar_hot_attr || 0;
	this.current = (this.items.length && this.cmd !== false) ? 0 : -1;
	var first = (this.cmd === undefined) ? console.bar_default : this.cmd;
	for (i = 0; i < this.items.length; i++) {
		if (this.items[i].cmd == first)
			this.current = i;
	}
	if (this.hot_attr) {	// the file was displayed without the command keys highlighted
		for (i = 0; i < this.items.length; i++) {
			if (i != this.current)
				this.draw(i, false);
		}
	}
	this.draw(this.current, true);
	return this.items.length;
};

// Without an @BARATTR code, a selected item is its own colors swapped
function reversed(attr)
{
	var fg = attr & 0x07, bg = (attr >> 4) & 0x07;

	if (fg == bg)
		return 0x70;
	return (fg << 4) | bg;
}

// Display an item's text as the file did, but with its command key (the first
// occurrence of a single-character command in the text) in the hot attribute's
// foreground color
MenuBar.prototype.print = function(part)
{
	var text = part.text;
	var i, attr;

	if (this.hot_attr && part.cmd.length == 1) {
		for (i = 0; i < text.length; i++) {
			if (text.charAt(i) == '\x01') {	// skip a Ctrl-A code
				i++;
				continue;
			}
			if (text.charAt(i).toUpperCase() == part.cmd.toUpperCase()) {
				console.print(text.substr(0, i));
				attr = console.attributes;
				console.attributes = (attr & ~0x0f) | (this.hot_attr & 0x0f);
				console.print(text.charAt(i));
				console.attributes = attr;
				console.print(text.substr(i + 1));
				return;
			}
		}
	}
	console.print(text);
};

MenuBar.prototype.draw = function(index, selected)
{
	var item = this.items[index];
	var attr = console.attributes;
	var i, part;

	if (!item)
		return;
	console.pushxy();
	for (i = 0; i < item.parts.length; i++) {
		part = item.parts[i];
		console.gotoxy(part.x, part.y);
		if (selected) {
			console.attributes = this.attr || reversed(part.attr);
			console.print(strip_ctrl_a(part.text));
		} else {
			console.attributes = part.attr;
			this.print(part);
		}
	}
	console.popxy();
	console.attributes = attr;
};

MenuBar.prototype.select = function(index)
{
	if (index == this.current || !this.items[index])
		return;
	this.draw(this.current, false);
	this.current = index;
	this.cmd = this.items[index].cmd;
	this.draw(index, true);
};

// Move to the nearest item in the direction (dx, dy); with nothing further
// that way, wrap around to the far end of the same line of items
MenuBar.prototype.move = function(dx, dy)
{
	var cur = this.items[this.current];
	var best = -1, wrap = -1, best_score, wrap_score;
	var i, along, across, score;

	if (!cur) {	// nothing highlighted yet
		this.select((dx < 0 || dy < 0) ? this.items.length - 1 : 0);
		return;
	}
	for (i = 0; i < this.items.length; i++) {
		if (i == this.current)
			continue;
		// A character cell is about twice as tall as it is wide
		along = dx ? (this.items[i].x - cur.x) * dx : (this.items[i].y - cur.y) * dy * 2;
		across = dx ? Math.abs(this.items[i].y - cur.y) * 2 : Math.abs(this.items[i].x - cur.x);
		if (along > 0) {
			score = along + across * 2;
			if (best < 0 || score < best_score) {
				best = i;
				best_score = score;
			}
		} else if (along < 0) {
			score = along + across * 2;
			if (wrap < 0 || score < wrap_score) {
				wrap = i;
				wrap_score = score;
			}
		}
	}
	this.select(best >= 0 ? best : wrap);
};

// Returns '' for a key that moved the bar, the current item's command for
// Enter, and any other key unchanged (a hot-key also moves the bar to its item)
MenuBar.prototype.handle = function(key)
{
	var i;

	if (!key || !this.items.length)
		return key;
	switch (key) {
		case KEY_UP:
			this.move(0, -1);
			return '';
		case KEY_DOWN:
			this.move(0, 1);
			return '';
		case KEY_LEFT:
			this.move(-1, 0);
			return '';
		case KEY_RIGHT:
			this.move(1, 0);
			return '';
		case KEY_HOME:
			this.select(0);
			return '';
		case KEY_END:
			this.select(this.items.length - 1);
			return '';
		case '\r':
			if (this.current < 0)
				return key;
			return this.items[this.current].cmd;
	}
	for (i = 0; i < this.items.length; i++) {
		if (this.items[i].cmd.toUpperCase() == key.toUpperCase()) {
			this.select(i);
			break;
		}
	}
	return key;
};

// Wait for one of 'keys' (any key when omitted), moving the bar meanwhile;
// returns '' on disconnect
MenuBar.prototype.getkey = function(keys, mode)
{
	var key;

	while (bbs.online && !js.terminated) {
		key = this.handle(console.getkey(mode === undefined ? K_UPPER : mode));
		if (key === '')
			continue;
		if (!keys || keys.indexOf(key) >= 0)
			return key;
	}
	return '';
};

this;
