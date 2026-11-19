// Library for the List Files loadable module (listfiles.js): option
// defaults and merging, header/footer menu file lookup, filespec widening,
// module-local @-codes, and the window of files on the screen
// for Synchronet v3.22+

// Usage: load("listfiles_lib.js");

// The [listfiles] options and their defaults, which reproduce the built-in
// file lister's output. An empty color (*_attr) option means the attr.ini color.
function listfiles_defaults()
{
	return {
		// One file's line, for users with batch flagging enabled and without
		line_fmt: "@NAME_ATTR@@NAME@@FLAGS@@LETTER_ATTR@@LETTER@@SIZE_ATTR@@SIZE-R6@@MISSING@@DESC_ATTR@@DESC@",
		line_fmt_nobatch: "@NAME_ATTR@@NAME@@FLAGS@@SIZE_ATTR@@SIZE-R7@@MISSING@@DESC_ATTR@@DESC@",
		// For batch flagging on terminals that position the cursor (lightbar):
		// the tagged/untagged marker in place of the letter
		line_fmt_interactive: "@NAME_ATTR@@NAME@@FLAGS@@LETTER_ATTR@@TAG@@SIZE_ATTR@@SIZE-R6@@MISSING@@DESC_ATTR@@DESC@",
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
		untagged_marker: " ",
		name_attr: "",
		size_attr: "",
		desc_attr: "",
		letter_attr: "",
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
		selected_item_attr: "\x01V",
		lightbar: true
	};
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
	"NAME_ATTR", "LETTER_ATTR", "SIZE_ATTR", "DESC_ATTR"
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
