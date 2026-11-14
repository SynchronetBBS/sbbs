// Numbered-list selection with paging and arrow-key navigation
// for Synchronet v3.22+ (uses console.getnum()'s navkeys argument)

// Used by uselect.js and by the area selection functions of shell_lib.js

// Usage: require("select_item.js", "select_item");
//        var index = select_item(opts);
//
// Returns the 0-based index of the selected item, -1 if the user quit
// (quit key or Ctrl-C), or -2 if the user hit Enter while no item was the
// default (opts.current < 0, nothing selected with the arrow keys).
//
// opts:
// - count       number of items (required)
// - current     0-based index of the default item (-1 for none)
// - header      string printed above the list, e.g. a heading
// - prompt      function(num) returning the prompt string (displayed via
//               console.mnemonics) where num is the 1-based default item
// - print_item  function(index, is_default, redraw) printing one item (one
//               line, WITHOUT a line ending) and adding its hotspot, unless
//               redraw is true (the item is being repainted in place, where
//               its hotspot already is)
// - menu        optional function() that displays a custom menu file in
//               place of the list, returning true if it did (the arrow keys
//               are then not supported since there's no list to update)
// - more_fmt    optional format string for the indicator shown when the
//               items don't fit on one screen: first, last, total
// - more_indent optional indent (in columns) of the default indicator, e.g.
//               to align it with the items after a marker (default: 2)

require("key_defs.js", "KEY_UP");

function select_item_count_lines(str)
{
	return str.split('\n').length - 1;
}

function select_item(opts)
{
	var count = opts.count;
	var cur = (opts.current === undefined) ? -1 : opts.current;
	var top = 0;		// index of first item displayed (when paging)
	var page = 0;		// number of items displayed
	var drawn = false;	// the list (not a custom menu) is on the screen
	var item_row = [];	// screen row (0-based) of each displayed item
	var first_row;		// screen row of the first displayed item
	var more_row;		// screen row of the "more" indicator
	var prompt_row;		// screen row of the last line of the prompt
	var prev;			// previous default item, when only it and cur need repainting

	if (count < 1)
		return -1;
	if (cur >= count)
		cur = -1;
	while (bbs.online && !js.terminated) {
		var udflt = (cur < 0) ? 1 : cur + 1;
		var prompt = opts.prompt(udflt);

		if (drawn && prev !== undefined && repaint(prev, cur)) {
			prev = undefined;
		} else if (drawn && page < count && repaint_page(udflt)) {
			prev = undefined;
		} else {
			drawn = false;
			console.clear(); // also clears hotspots
			if (!(opts.menu && opts.menu())) {
				// The screen was just cleared, so drop any leading blank lines
				var header = (opts.header || "").replace(/^[\r\n]+/, "");
				// Rows available for items: the screen, less the header, the prompt
				// (and the cursor line it ends on)
				var rows = console.screen_rows - select_item_count_lines(header)
					- select_item_count_lines(prompt) - 1;
				// The "more" indicator goes on the prompt's leading blank line, if it has one
				var more_on_blank = /^\r?\n/.test(prompt);
				page = count;
				if (page > rows)
					page = Math.max(more_on_blank ? rows : rows - 1, 1);
				scroll_to(udflt);

				console.print(header);
				console.line_counter = 0;
				first_row = console.current_row;
				item_row = [];
				for (var i = top; i < count && i < top + page; ++i) {
					item_row[i] = console.current_row;
					opts.print_item(i, i == cur, false);
					console.newline();
				}
				if (page < count) {
					more_row = console.current_row;
					console.print(more_str());
					if (!more_on_blank)
						console.newline();
				}
				drawn = true;
			}
			console.line_counter = 0;
			console.mnemonics(prompt);
			prompt_row = console.current_row;
		}
		var choice = console.getnum(count, 0, drawn);
		if (typeof choice === 'string') {
			var next = udflt - 1;
			var old_top = top;
			if (choice == KEY_UP)
				next--;
			else if (choice == KEY_DOWN)
				next++;
			else if (choice == KEY_PAGEUP) {
				next -= page;
				top -= page;
			} else if (choice == KEY_PAGEDN) {
				next += page;
				top += page;
			} else if (choice == KEY_HOME)
				next = 0;
			else if (choice == KEY_END)
				next = count - 1;
			next = Math.max(0, Math.min(next, count - 1));
			top = Math.max(0, Math.min(top, count - page));
			// If the page didn't change, only the previous and new default
			// items (and the prompt) need repainting
			prev = (top == old_top) ? cur : undefined;
			cur = next;
			continue;
		}
		console.clear_hotspots();
		if (choice < 0)
			return -1;
		if (choice == 0)
			return (cur < 0) ? -2 : cur;
		return choice - 1;
	}
	return -1;

	// Scroll the page so the default item (1-based) is visible
	function scroll_to(udflt)
	{
		if (udflt - 1 < top)
			top = udflt - 1;
		else if (udflt - 1 >= top + page)
			top = udflt - page;
		top = Math.max(0, Math.min(top, count - page));
	}

	function more_str()
	{
		if (opts.more_fmt)
			return format(opts.more_fmt, top + 1, top + page, count);
		var indent = (opts.more_indent === undefined) ? 2 : opts.more_indent;
		return "\x01n\x01h\x01k" + format("%*s[%u-%u of %u]", indent, "", top + 1, top + page, count);
	}

	// Repaint every item line of the page in place (after a page change),
	// the "more" indicator and the prompt, without clearing the screen,
	// returning false if a full redraw is required
	function repaint_page(udflt)
	{
		scroll_to(udflt);
		console.clear_hotspots(); // the items are moving to different rows
		item_row = [];
		for (var i = top; i < top + page; ++i) {
			var row = first_row + (i - top);
			if (!goto_line(row))
				return false;
			item_row[i] = row;
			opts.print_item(i, i == cur, false);
			end_line();
		}
		if (!goto_line(more_row))
			return false;
		console.print(more_str());
		end_line();
		return repaint_prompt(udflt);
	}

	// Repaint the previous and new default items in place, and the prompt,
	// returning false if a full redraw is required (e.g. the new default item
	// isn't on the displayed page or the terminal can't position the cursor)
	function repaint(prev, cur)
	{
		if (item_row[cur] === undefined)
			return false;
		if (prev != cur) {
			if (prev >= 0 && item_row[prev] !== undefined) {
				if (!goto_line(item_row[prev]))
					return false;
				opts.print_item(prev, false, true);
				end_line();
			}
			if (!goto_line(item_row[cur]))
				return false;
			opts.print_item(cur, true, true);
			end_line();
		}
		return repaint_prompt(cur + 1);
	}

	// Repaint the last line of the prompt (with the new default item number)
	function repaint_prompt(udflt)
	{
		if (!goto_line(prompt_row))
			return false;
		var prompt = opts.prompt(udflt);
		console.mnemonics(prompt.substr(prompt.lastIndexOf('\n') + 1));
		// Clear what's left of a longer previous prompt, keeping the prompt's
		// attributes for the echo of the user's input
		var attr = console.attributes;
		console.print("\x01n");
		console.cleartoeol();
		console.attributes = attr;
		return true;
	}

	// Move the cursor to the start of a screen row (0-based) to overwrite it
	function goto_line(row)
	{
		return console.gotoxy(1, row + 1);
	}

	// Clear the rest of a line just overwritten (in the normal attribute)
	function end_line()
	{
		console.print("\x01n");
		console.cleartoeol();
	}
}
