// Library for the List Files loadable module (listfiles.js): option
// defaults and merging, header/footer menu file lookup, filespec widening,
// module-local @-codes, and pagination
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
		extdesc_marker: "+",
		missing_marker: "-",
		tagged_marker: "*",
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
		lightbar: true,
		more_fmt: "\x01n\x01h\x01k   [%u-%u of %u]"
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

// Split items (files) of the given screen-line heights into pages of at most
// 'rows' lines each, in order: [{ first: index, count: n }, ...]. An item
// taller than a page gets a page of its own (and is clipped when displayed).
function listfiles_paginate(heights, rows)
{
	var pages = [];
	var page = null;
	var used = 0;
	for (var i = 0; i < heights.length; i++) {
		var h = Math.min(heights[i], rows);
		if (page === null || used + h > rows) {
			page = { first: i, count: 0 };
			pages.push(page);
			used = 0;
		}
		page.count++;
		used += h;
	}
	return pages;
}

this;
