// Library for the List Files loadable module (listfiles.js): option
// defaults and merging, header/footer menu file lookup, filespec widening,
// module-local @-codes, and the window of files on the screen
// for Synchronet v3.22+

// Usage: load("listfiles_lib.js");

require("attr_option.js", "attr_option");

// The [listfiles] options and their defaults, which reproduce the built-in
// file lister's output (except for the colors of the extended description
// marker and the tagged marker). The color (*_attr) options are attribute
// letters, as in attr.ini (e.g. "WH4": bright white on blue; converted by
// attr_options()); an empty one means the attr.ini color.
function listfiles_defaults()
{
	return {
		// One file's line, for users with batch flagging enabled and without
		line_fmt: "@NAME_ATTR@@NAME@@FLAGS_ATTR@@FLAGS@@LETTER_ATTR@@LETTER@@SIZE_ATTR@@SIZE-R6@@MISSING@@DESC_ATTR@@DESC@",
		line_fmt_nobatch: "@NAME_ATTR@@NAME@@FLAGS_ATTR@@FLAGS@@SIZE_ATTR@@SIZE-R7@@MISSING@@DESC_ATTR@@DESC@",
		// For batch flagging on terminals that position the cursor (lightbar):
		// the tagged/untagged marker in place of the letter
		line_fmt_interactive: "@NAME_ATTR@@NAME@@FLAGS_ATTR@@FLAGS@@TAG_ATTR@@TAG@@SIZE_ATTR@@SIZE-R6@@MISSING@@DESC_ATTR@@DESC@",
		// The command keys shown above the files in the interactive mode, below
		// any header ("" for none, e.g. when a custom header shows them)
		interactive_commands: "\x01n\x01>\x01q\x014 \x01h\x01y[\x01w\x01~D\x01y]ownload  [\x01w\x01~B\x01y]atch  [\x01wSpace\x01y] Tag  [\x01w\x01~V\x01y]iew  [\x01w\x01~E\x01y]xt Info  [\x01w\x01~T\x01y]oggle Desc  [\x01w\x01~Q\x01y]uit\x01>\x01n\r\n",
		// The interactive mode's prompt. Arguments: more_above or no_more,
		// more_below or no_more, library number, library name, directory
		// number, directory name, number of files
		interactive_prompt: "\x01n\x01c%s \x01hListing \x01n\x01c%s (\x01h%u\x01n\x01c) %s (\x01h%u\x01n\x01c) %s (\x01h\x01`?\x01n\x01c=Menu) (\x01h%u\x01n\x01c Files): \x01n\x01~",
		// Right-justified on the line above the interactive mode's prompt
		// ("" for none). Arguments: current file number, number of files
		position_fmt: "\x01n\x01h\x01k[file %u of %u]",
		// Whether there are more pages of files before and after this one,
		// on either side of the prompt's verb (as msglist.js's reader shows)
		more_above: "\x18",
		more_below: "\x19",
		no_more: "\xfe",
		extdesc_marker: "+",
		missing_marker: "-",
		tagged_marker: "\xfb",	// CP437 check mark, as msglist.js uses
		queued_marker: "\x19",	// a down arrow: a file in the batch download queue (interactive styles)
		untagged_marker: " ",
		name_attr: "",
		size_attr: "",
		desc_attr: "",
		letter_attr: "",
		tag_attr: "WH",	// the tagged marker's color ("": the filename color)
		flags_attr: "KH",	// the extended description marker's color ("": the filename color)
		free_attr: "",
		hdr_box_attr: "",
		hdr_line_attr: "",
		box_top_left: "\xc9",
		box_top_right: "\xbb",
		box_bottom_left: "\xc8",
		box_bottom_right: "\xbc",
		box_horizontal: "\xcd",
		box_vertical: "\xba",
		short_hdr_underline: "\xc4",
		selected_item_attr: "V",	// the current file's colors, in the classic style
		highlight_line: true,	// the current file's whole line, or (false) only its name, in the classic style
		// The interactive style: "classic" (like the built-in lister) or
		// "lightbar" (an area line on top, key tips on the bottom row, a
		// preview pane)
		style: "classic",
		// The lightbar style's file line, area line (arguments: library name,
		// directory's long name), position (current file number, number of
		// files), column headings (true: from the file line and the heading_* titles,
		// in lightbar_headings_attr; false for none; or text; the area line's
		// and headings' last color fills the rest of their rows, as in
		// msglist.js) and key tips:
		// a mnemonics string, ~ before each key letter (the second for users
		// who may remove files)
		line_fmt_lightbar: "@NAME_ATTR@@NAME@@FLAGS_ATTR@@FLAGS@@TAG_ATTR@@TAG@@SIZE_ATTR@@SIZE-R6@@MISSING@@DESC_ATTR@@DESC@",
		lightbar_area_fmt: "\x01n\x01k\x017 %s / %s\x01h",
		lightbar_position_fmt: "[file %u of %u]",
		lightbar_headings: true,	// true: from line_fmt_lightbar and the heading_* titles
		lightbar_headings_attr: "KH7",
		heading_name: "Name",
		heading_size: "Size",
		heading_desc: "Description",
		// (printed before the key tips: Ctrl-A codes in the tips themselves
		// would turn off console.mnemonics()' coloring of the keys)
		lightbar_keys_prefix: "\x01n\x01c\xfe ",
		lightbar_keys: "~Download, ~Batch, ~View, ~Extended, ~Info, Space=Tag, ~Quit or ~?",
		lightbar_keys_operator: "~Download, ~Batch, ~View, ~Extended, ~Info, ~Remove, ~Move, Space=Tag, ~Quit or ~?",
		lightbar_attr: "WH4",
		// The key tips row's colors ("" for attr.ini's mnemonic colors, the
		// row not filled) and its key letters' colors (e.g. K6 and WH6)
		lightbar_keys_attr: "",
		lightbar_key_attr: "",
		// The preview pane: the file's details (@FILE_*@ codes; the extended
		// description follows)
		// The full filename, first in the pane when the list shows it cut short
		preview_name_fmt: "\x01n\x01cName \x01h@FILE_NAME@\x01n\r\n",
		preview_info_fmt: "\x01n\x01cSize \x01h@FILE_BYTES-T@\x01n\x01c bytes, uploaded \x01h@FILE_DATE_ULED@\x01n\x01c by \x01h@FILE_UPLOADER@\r\n"
			+ "\x01n\x01cDownloaded \x01h@FILE_TIMES_DLED-T@\x01n\x01c times, last on \x01h@FILE_DATE_DLED@\x01n\x01c, cost \x01h@FILE_COST@\x01n\r\n",
		// The pane's separator line: its fill, label, line range (first
		// line, last line, number of lines) and colors (without and with the
		// focus)
		preview_separator: "\xc4",
		preview_info_label: "\xb4 Info \xc3",
		preview_lines_fmt: "\xb4 Lines %u-%u of %u \xc3",
		preview_extdesc_attr: "WH",	// the extended description's color (before its own codes)
		preview_attr: "C",
		preview_focus_attr: "WH"
	};
}

// Key tips (a mnemonics string, ~ before each key letter) in the colors
// 'bar_attr', with each key letter in 'key_attr' (Ctrl-A codes), pushing and
// popping the colors around it; unchanged when 'bar_attr' is empty (for
// console.mnemonics()' own colors)
function listfiles_key_tips(keys, bar_attr, key_attr)
{
	if (!bar_attr)
		return keys;
	return bar_attr + keys.replace(/~(.)/g, function (match, key) {
		return "\x01+" + key_attr + "~" + key + "\x01-";
	});
}

// Text without its Ctrl-A codes, keeping any other control-range characters
// (unlike strip_ctrl(), which removes the CP437 graphics there, e.g. the down
// arrow, \x19)
function listfiles_strip_colors(text)
{
	return text.replace(/\x01./g, "");
}

// The lowercase names of the files in the batch download queue from the
// directory with internal code 'code', as keys of an object: 'entries' as
// bbs.batch_dnload_list() returns them (each with the filename, "name", and
// the directory's internal code, "dir")
function listfiles_queued(entries, code)
{
	var result = {};
	if (!entries)
		return result;
	code = code.toLowerCase();
	for (var i = 0; i < entries.length; i++) {
		if (entries[i].dir && String(entries[i].dir).toLowerCase() === code)
			result[String(entries[i].name).toLowerCase()] = true;
	}
	return result;
}

// Merge option objects, later ones overriding earlier ones (null or
// undefined arguments are skipped)
function listfiles_options(defaults, base, lib_section, dir_section)
{
	var result = {};
	for (var i = 0; i < arguments.length; i++) {
		var obj = arguments[i];
		if (obj === null || obj === undefined)
			continue;
		for (var key in obj)
			result[key] = obj[key];
	}
	return result;
}

// Menu file names (in text/menu) to try for a directory's header ("head") or
// footer ("tail"), most specific first: directory, library, all
function listfiles_menu_candidates(dircode, libnum, which)
{
	return [
		"listfiles_" + dircode.toLowerCase() + "_" + which,
		"listfiles_lib" + libnum + "_" + which,
		"listfiles_" + which
	];
}

// A filespec of 12 or more characters without wildcards also matches longer
// names: "*" is inserted before its extension (or appended)
function listfiles_widen_filespec(spec)
{
	if (spec.length < 12 || /[*?]/.test(spec))
		return spec;
	var dot = spec.lastIndexOf(".");
	if (dot < 0)
		return spec + "*";
	return spec.substr(0, dot) + "*" + spec.substr(dot);
}

// A filename formatted to exactly 'size' columns, as the built-in file
// lister does: the extension kept, the rest shortened or space-padded
function listfiles_format_filename(name, size)
{
	var result;
	var dot = name.lastIndexOf(".");
	if (dot < 0)
		result = name.substr(0, size);
	else {
		var ext = name.substr(dot);
		if (ext.length >= size)
			result = name.substr(0, size);
		else {
			var base = name.substr(0, Math.min(dot, size - ext.length));
			while (base.length < size - ext.length)
				base += " ";
			result = base + ext;
		}
	}
	while (result.length < size)
		result += " ";
	return result;
}

// Whether the preview pane should show the full filename 'name': when its
// column of 'size' characters cuts it short (see listfiles_format_filename())
// and the description 'desc' doesn't already show it (an empty description
// is listed as the filename)
function listfiles_pane_shows_name(name, desc, size)
{
	desc = (desc || "").replace(/^\s+|\s+$/g, "");
	return name.length > size && desc !== "" && desc !== name;
}

// A byte count estimated in at most 6 characters (e.g. "1023", "1.5K",
// "976.6K", "10.0G"), as in the built-in file lister's size column
function listfiles_size_str(bytes)
{
	var KB = 1024, MB = KB * KB, GB = MB * KB, TB = GB * KB, PB = TB * KB;
	var str;
	for (var unit = 1; ; unit *= 1024) {
		if (bytes >= PB)
			str = format("%.1fP", bytes / PB);
		else if (bytes >= TB || unit == TB)
			str = format("%.1fT", bytes / TB);
		else if (bytes >= GB || unit == GB)
			str = format("%.1fG", bytes / GB);
		else if (bytes >= MB || unit == MB)
			str = format("%.1fM", bytes / MB);
		else if (bytes >= KB || unit == KB)
			str = format("%.1fK", bytes / KB);
		else
			str = String(bytes);
		if (str.length <= 6 || unit >= TB)
			return str;
	}
}

// Ctrl-A codes for an IBM CGA attribute value (e.g. from console.color_list[])
function listfiles_attr_str(attr)
{
	var colors = "kbgcrmyw";
	var str = "\x01n\x01" + colors.charAt(attr & 7);
	if (attr & 0x08)
		str += "\x01h";
	if ((attr >> 4) & 7)	// background digits are in ANSI (not CGA) color order
		str += "\x01" + "04261537".charAt((attr >> 4) & 7);
	if (attr & 0x80)
		str += "\x01i";
	return str;
}

// Codes expanded by the module itself (each from the like-named, lowercase
// property of the 'values' passed to listfiles_local_codes())
var LISTFILES_LOCAL_CODES = [
	"LETTER", "TAG", "FLAGS", "MISSING", "SIZE", "NAME", "DESC",
	"NAME_ATTR", "FLAGS_ATTR", "LETTER_ATTR", "TAG_ATTR", "SIZE_ATTR", "DESC_ATTR"
];

// Expand the module-local @-codes in 'str', with the same width/alignment
// modifiers as Synchronet's @-codes (-L<n>, -R<n>, -C<n> or |L<n> etc.), a
// modifier with no width meaning the width of the code itself. Other @-codes
// are left for bbs.expand_atcodes().
function listfiles_local_codes(str, values)
{
	var re = new RegExp("@(" + LISTFILES_LOCAL_CODES.join("|") + ")(?:-([LRC])|\\|([A-Z]*))?(\\d*)@", "g");
	return str.replace(re, function (match, code, dash_align, bar_flags, width) {
		var value = values[code.toLowerCase()];
		if (value === undefined || value === null)
			value = "";
		value = String(value);
		var align = dash_align;
		if (!align && bar_flags) {
			var m = bar_flags.match(/[LRC]/);
			align = m ? m[0] : undefined;
		}
		if (!align)
			return value;
		var len = width ? parseInt(width, 10) : match.length;
		if (value.length >= len)
			return value.substr(0, len);
		var fill = len - value.length;
		switch (align) {
			case "L":
				return value + format("%*s", fill, "");
			case "R":
				return format("%*s", fill, "") + value;
		}
		var left = Math.floor(fill / 2);
		return format("%*s", left, "") + value + format("%*s", fill - left, "");
	});
}

// Expand a file line template: the module-local @-codes from 'values', and
// the rest of the template's text by expander(text) (e.g. a wrapper for
// bbs.expand_atcodes()), called only on text with an '@' in it. The values
// (e.g. an uploader's description) never pass through the expander.
function listfiles_expand(fmt, values, expander)
{
	var re = new RegExp("@(?:" + LISTFILES_LOCAL_CODES.join("|") + ")(?:-[LRC]|\\|[A-Z]*)?\\d*@", "g");
	var result = "";
	var last = 0;
	var match;
	function literal(text)
	{
		return text.indexOf("@") < 0 ? text : expander(text);
	}
	while ((match = re.exec(fmt)) !== null) {
		result += literal(fmt.substring(last, match.index));
		result += listfiles_local_codes(match[0], values);
		last = re.lastIndex;
	}
	return result + literal(fmt.substring(last));
}

// The text render(names) (e.g. a prompt with a library and directory name)
// shortened to fit in 'width' columns, as measured by measure(text), by
// shortening the longest of the names a character at a time (down to one
// character each), so that what follows the names stays in view
function listfiles_shorten_to_fit(names, render, measure, width)
{
	names = names.slice();
	var text = render(names);
	while (measure(text) > width) {
		var longest = 0;
		for (var i = 1; i < names.length; i++) {
			if (names[i].length >= names[longest].length)
				longest = i;
		}
		if (names[longest].length <= 1)
			break;
		names[longest] = names[longest].substr(0, names[longest].length - 1);
		text = render(names);
	}
	return text;
}

// The screen lines of an extended description: split at its line breaks
// (CR/LF) and at its Ctrl-A conditional newlines (\x01/), which break only
// once something visible is on the line. Leading blank lines and
// normal-attribute codes, and trailing white space, are dropped.
function listfiles_extdesc_lines(ext)
{
	var text = ext.replace(/\s+$/, "").replace(/^(\r\n|\x01[nN0wW])+/, "");
	var result = [];
	var lines = text.split(/\r?\n/);
	for (var i = 0; i < lines.length; i++) {
		var pieces = lines[i].split("\x01/");
		var line = pieces[0];
		var broken = false;
		for (var k = 1; k < pieces.length; k++) {
			if (strip_ctrl(line) !== "") {
				result.push(line);
				line = "";
				broken = true;
			}
			line += pieces[k];
		}
		if (line !== "" || !broken)	// (nothing after a conditional newline: no line)
			result.push(line);
	}
	return result;
}

// Which listing a user gets: "classic" or "lightbar" (the interactive
// styles, for users with batch flagging enabled on a terminal that
// positions the cursor; 'style' is the style option), "letters" (batch
// flagging on another terminal) or "plain"
function listfiles_style(style, batch, cursor)
{
	if (!batch)
		return "plain";
	if (!cursor)
		return "letters";
	return style === "lightbar" ? "lightbar" : "classic";
}

// The fewest screen rows with room for the lightbar style's preview pane
var LISTFILES_PANE_MIN_ROWS = 20;

// The lightbar style's screen rows (0-based): the area line on row 0, any
// column headings on row 1, the key tips on the last row, and between them
// the list and, if 'pane' and the screen has at least
// LISTFILES_PANE_MIN_ROWS rows, a separator and the preview pane
function listfiles_lightbar_rows(screen_rows, headings, pane)
{
	var top = headings ? 2 : 1;
	var avail = screen_rows - top - 1;
	var rows = { list_top: top, list_rows: avail, pane_top: 0, pane_rows: 0 };
	if (!pane || screen_rows < LISTFILES_PANE_MIN_ROWS)
		return rows;
	var list_rows = Math.ceil(avail / 2);
	var pane_rows = avail - list_rows - 1;
	if (list_rows < 3 || pane_rows < 3)
		return rows;
	rows.list_rows = list_rows;
	rows.pane_top = top + list_rows + 1;
	rows.pane_rows = pane_rows;
	return rows;
}

// The preview pane's first line shown after a scrolling key ("up", "down",
// "pgup", "pgdn", "home" or "end"), for 'total' lines in 'rows' rows
function listfiles_pane_scroll(scroll, key, total, rows)
{
	var last = Math.max(0, total - rows);
	switch (key) {
		case "up":		scroll--; break;
		case "down":	scroll++; break;
		case "pgup":	scroll -= rows; break;
		case "pgdn":	scroll += rows; break;
		case "home":	scroll = 0; break;
		case "end":		scroll = last; break;
	}
	return Math.max(0, Math.min(last, scroll));
}

// Column headings for a file line template 'fmt': the template with the
// titles (name, size, desc) in place of the filename (centered in 'namelen'
// columns, the filenames' width), size and description, blanks of the same
// widths in place of other @-codes (a modifier without a width meaning the
// code's own width; empty with no modifier), and no colors
function listfiles_headings(fmt, titles, namelen)
{
	var values = {};
	for (var i = 0; i < LISTFILES_LOCAL_CODES.length; i++)
		values[LISTFILES_LOCAL_CODES[i].toLowerCase()] = "";
	values.flags = values.tag = values.letter = values.missing = " ";
	var left = Math.max(0, Math.floor((namelen - titles.name.length) / 2));	// centered
	values.name = format("%-*s", namelen, format("%*s", left, "") + titles.name);
	values.size = titles.size;
	values.desc = titles.desc;
	var text = listfiles_local_codes(fmt, values);
	text = text.replace(/@[A-Za-z0-9_:]+(-[LRC]|\|[A-Z]*)?(\d*)@/g, function (match, modifier, width) {
		var len = width ? parseInt(width, 10) : (modifier ? match.length : 0);
		return format("%*s", len, "");
	});
	return strip_ctrl(text);
}

// A mnemonics string (~ before each key letter, as console.mnemonics()
// displays) cut to 'width' displayed columns (the ~ and Ctrl-A codes take
// none)
function listfiles_mnemonics_cut(str, width)
{
	var out = "";
	var cols = 0;
	for (var i = 0; i < str.length; i++) {
		var ch = str.charAt(i);
		if (ch === "\x01" && i + 1 < str.length) {
			out += ch + str.charAt(++i);
			continue;
		}
		if (ch === "~" && i + 1 < str.length) {
			if (cols + 1 > width)
				break;
			out += ch + str.charAt(++i);
			cols++;
			continue;
		}
		if (cols + 1 > width)
			break;
		out += ch;
		cols++;
	}
	return out;
}

// A window of items (files) of the given screen-line heights, with at most
// 'rows' lines on the screen: an item taller than that counts as 'rows'
// lines (it's clipped when displayed)

// The number of items, starting with item 'top', that fit in the window
// (at least one, unless there are none)
function listfiles_window_count(heights, top, rows)
{
	var count = 0;
	var used = 0;
	for (var i = top; i < heights.length; i++) {
		var h = Math.min(heights[i], rows);
		if (count && used + h > rows)
			break;
		used += h;
		count++;
	}
	return count;
}

// The first item of a window whose last item is 'bottom' (the window
// reaching as far up as fits)
function listfiles_window_top(heights, bottom, rows)
{
	var top = bottom;
	var used = Math.min(heights[bottom], rows);
	while (top > 0 && used + Math.min(heights[top - 1], rows) <= rows) {
		top--;
		used += Math.min(heights[top], rows);
	}
	return top;
}

this;
