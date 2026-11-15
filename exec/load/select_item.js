// Numbered-list selection with paging and arrow-key navigation
// for Synchronet v3.22+

// Used by uselect.js, text_sec.js, xtrn_sec.js and by the area selection
// functions of shell_lib.js

// Usage: require("select_item.js", "select_item");
//        var index = select_item(opts);
//
// Returns the 0-based index of the selected item, -1 if the user quit
// (quit key or Ctrl-C), or -2 if the user hit Enter while no item was the
// default (opts.current < 0, nothing selected with the arrow keys), or one of
// the opts.keys command keys (a string).
//
// opts:
// - count       number of items (required)
// - current     0-based index of the default item (-1 for none)
// - header      string printed above the list, e.g. a heading, or a
//               function() that displays it
// - footer      optional function() that displays what goes between the list
//               and the prompt (its lines are deducted from the page height)
// - prompt      function(num) returning the prompt string (displayed via
//               console.mnemonics) where num is the 1-based default item
// - print_item  function(index, is_default, redraw) printing one item (one
//               line, WITHOUT a line ending) and adding its hotspot, unless
//               redraw is true (the item is being repainted in place, where
//               its hotspot already is)
// - columns     optional number of columns (default: 1); the items are listed
//               down the first column, then the next, each item printed the
//               same width, and the left and right arrow keys move between
//               the columns
// - column_separator  optional string printed between columns (default: " ")
// - menu        optional function() that displays a custom menu file in
//               place of the list, returning true if it did (the arrow keys
//               are then not supported since there's no list to update)
// - more_fmt    optional format string for the indicator shown when the
//               items don't fit on one screen: first, last, total
// - more_indent optional indent (in columns) of the default indicator, e.g.
//               to align it with the items after a marker (default: 2)
// - keys        optional string of additional command keys (uppercase), any
//               of which is returned to the caller, with opts.current set to
//               the default item at the time
// - no_default  true when the list doesn't mark a default item: the
//               navigation keys then only scroll a list longer than the
//               screen, and Enter returns -2
// - no_clear    true to display the list without clearing the screen first:
//               the list is then not paged and the navigation keys are not
//               supported (where the list is on the screen isn't known)

require("sbbsdefs.js", "K_NOCRLF");
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
	var page = 0;		// number of items per page
	var visible = 0;	// number of items displayed
	var per_col = 0;	// number of items displayed per column
	var cols = Math.max(opts.columns || 1, 1);
	var footer_rows = 0;	// screen rows taken by opts.footer(), once measured
	var footer_measured = false;
	var drawn = false;	// the list (not a custom menu) is on the screen
	var item_row = [];	// screen row (0-based) of each displayed item
	var item_col = [];	// screen column (0-based) of each displayed item
	var first_row;		// screen row of the first displayed item
	var more_row;		// screen row of the "more" indicator
	var prompt_row;		// screen row of the last line of the prompt
	var prev;			// previous default item, when only it and cur need repainting
	var idle = false;	// the last key changed nothing, so there's nothing to repaint
	var nav_keys = KEY_UP + KEY_DOWN + KEY_HOME + KEY_END + KEY_PAGEUP + KEY_PAGEDN;
	if (cols > 1)
		nav_keys += KEY_LEFT + KEY_RIGHT;

	if (count < 1)
		return -1;
	if (cur >= count || opts.no_default)
		cur = -1;
	while (bbs.online && !js.terminated) {
		var udflt = (cur < 0) ? 1 : cur + 1;
		var prompt = opts.prompt(udflt);

		if (idle) {
			idle = false;
		} else if (drawn && prev !== undefined && repaint(prev, cur)) {
			prev = undefined;
		} else if (drawn && page < count && repaint_page(udflt)) {
			prev = undefined;
		} else {
			drawn = false;
			prev = undefined;
			if (opts.no_clear)
				console.clear_hotspots();
			else
				console.clear(); // also clears hotspots
			if (!(opts.menu && opts.menu())) {
				var prompt_lines = select_item_count_lines(prompt);
				if (typeof opts.header == 'function')
					opts.header();
				else if (opts.no_clear)
					console.print(opts.header || "");
				else	// The screen was just cleared, so drop any leading blank lines
					console.print((opts.header || "").replace(/^[\r\n]+/, ""));
				console.line_counter = 0;
				// Rows available for items: the screen, less the header, the footer
				// and the prompt (and the cursor line it ends on)
				var rows = console.screen_rows - console.current_row - footer_rows - prompt_lines - 1;
				// The "more" indicator goes on the prompt's leading blank line, if it
				// has one (and there's no footer between them)
				var more_on_blank = !opts.footer && /^\r?\n/.test(prompt);
				page = count;
				if (!opts.no_clear && Math.ceil(count / cols) > rows)
					page = Math.max(more_on_blank ? rows : rows - 1, 1) * cols;
				scroll_to(udflt);

				first_row = console.current_row;
				item_row = [];
				item_col = [];
				visible = Math.min(page, count - top);
				per_col = Math.ceil(visible / cols);
				for (var r = 0; r < per_col; ++r) {
					for (var c = 0; c < cols; ++c) {
						var i = top + (c * per_col) + r;
						if (i >= top + visible)
							break;
						if (c)
							console.print(opts.column_separator === undefined ? " " : opts.column_separator);
						item_row[i] = console.current_row;
						item_col[i] = console.current_column;
						opts.print_item(i, i == cur, false);
					}
					console.newline();
					console.line_counter = 0;
				}
				if (page < count) {
					more_row = console.current_row;
					console.print(more_str());
					if (!more_on_blank)
						console.newline();
				}
				if (opts.footer) {
					opts.footer();
					// A footer that pushed the prompt off the bottom of the screen
					// is measured (on a cleared screen), for the list to be
					// redisplayed in the rows that leaves
					if (!footer_measured && !opts.no_clear
						&& console.current_row + prompt_lines > console.screen_rows - 1) {
						console.clear();
						opts.footer();
						footer_rows = console.current_row;
						footer_measured = true;
						continue;
					}
				}
				drawn = !opts.no_clear;
			}
			console.line_counter = 0;
			console.mnemonics(prompt);
			prompt_row = console.current_row;
		}
		var choice = get_choice();
		if (typeof choice !== 'string')
			end_prompt();
		if (typeof choice === 'object') {
			console.clear_hotspots();
			opts.current = cur;
			return choice.key;
		}
		if (typeof choice === 'string') {
			var next = udflt - 1;
			var old_top = top;
			if (opts.no_default) {
				// Columns scroll a page at a time
				if (choice == KEY_UP)
					top -= (cols > 1) ? page : 1;
				else if (choice == KEY_DOWN)
					top += (cols > 1) ? page : 1;
				else if (choice == KEY_PAGEUP)
					top -= page;
				else if (choice == KEY_PAGEDN)
					top += page;
				else if (choice == KEY_HOME)
					top = 0;
				else if (choice == KEY_END)
					top = count - 1;
				clamp_top();
				idle = (top == old_top);
				prev = undefined;
				continue;
			}
			if (choice == KEY_UP)
				next--;
			else if (choice == KEY_DOWN)
				next++;
			else if (choice == KEY_LEFT) {
				if (next - per_col >= top)
					next -= per_col;
			} else if (choice == KEY_RIGHT) {
				// From beside the empty bottom of the last column: to its last item
				var last = top + visible - 1;
				if (next + per_col <= last)
					next += per_col;
				else if (Math.floor((next - top) / per_col) < Math.floor((last - top) / per_col))
					next = last;
			} else if (choice == KEY_PAGEUP) {
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
			clamp_top();
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

	// Returns an item number, 0 for Enter, -1 for the quit key or Ctrl-C, a
	// navigation key (a string) or one of the caller's command keys as { key: }
	function get_choice()
	{
		var nav = drawn ? nav_keys : "";
		var key = console.getkeys(nav + console.quit_key + (opts.keys || ""), count, K_UPPER | K_NOCRLF);

		if (typeof key === 'number')
			return key > 0 ? key : -1;	// 0: aborted (Ctrl-C)
		if (key === "")	// Enter
			return 0;
		if (key == console.quit_key)
			return -1;
		if (nav.indexOf(key) >= 0)
			return key;
		return { key: key };
	}

	// End the prompt's line once the user has answered it. A full page leaves
	// the prompt on the bottom line of the screen, where a newline would
	// scroll the list, so that line is cleared for what's displayed next.
	function end_prompt()
	{
		console.print("\x01n");
		if (console.current_row < console.screen_rows - 1)
			console.newline();
		else {
			console.print("\r");
			console.cleartoeol();
		}
		console.line_counter = 0;
	}

	// Keep the first item displayed within the list: a single column scrolls
	// by the item and ends with a full page, while columns are displayed in
	// whole pages, the last of which may be a short one
	function clamp_top()
	{
		if (cols > 1)
			top = Math.floor(Math.max(0, Math.min(top, count - 1)) / page) * page;
		else
			top = Math.max(0, Math.min(top, count - page));
	}

	// Scroll the page so the default item (1-based) is visible
	function scroll_to(udflt)
	{
		if (!opts.no_default) {
			if (cols > 1)
				top = udflt - 1;
			else if (udflt - 1 < top)
				top = udflt - 1;
			else if (udflt - 1 >= top + page)
				top = udflt - page;
		}
		clamp_top();
	}

	function more_str()
	{
		var last = Math.min(top + page, count);
		if (opts.more_fmt)
			return format(opts.more_fmt, top + 1, last, count);
		var indent = (opts.more_indent === undefined) ? 2 : opts.more_indent;
		return "\x01n\x01h\x01k" + format("%*s[%u-%u of %u]", indent, "", top + 1, last, count);
	}

	// Repaint every item line of the page in place (after a page change),
	// the "more" indicator and the prompt, without clearing the screen,
	// returning false if a full redraw is required
	function repaint_page(udflt)
	{
		if (cols > 1)	// a short last page would leave items of the previous one
			return false;
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
				if (!goto_line(item_row[prev], item_col[prev]))
					return false;
				opts.print_item(prev, false, true);
				end_item(prev);
			}
			if (!goto_line(item_row[cur], item_col[cur]))
				return false;
			opts.print_item(cur, true, true);
			end_item(cur);
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

	// Move the cursor to (a column of) a screen row, both 0-based, to
	// overwrite what's there
	function goto_line(row, col)
	{
		return console.gotoxy((col || 0) + 1, row + 1);
	}

	// End an item just overwritten, clearing the rest of its line unless
	// there's an item in the next column
	function end_item(i)
	{
		if (i + per_col < top + visible)
			console.print("\x01n");
		else
			end_line();
	}

	// Clear the rest of a line just overwritten (in the normal attribute)
	function end_line()
	{
		console.print("\x01n");
		console.cleartoeol();
	}
}
