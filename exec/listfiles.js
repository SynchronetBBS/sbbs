// List Files loadable module: a customizable replacement for the built-in
// file lister (SCFG->System->Loadable Modules->List Files)
// for Synchronet v3.22+

// Invoked by the Terminal Server with arguments:
//   <dir code> <mode (FL_* flags)> [filespec or search expression]
// With no arguments, it lists every file of the user's current directory.
// Exits with the number of files listed, or -1 if the listing was aborted.

// Options: the [listfiles] section of ctrl/modopts.ini, overridden for one
// library by [listfiles:lib<N>] and for one directory by [listfiles:<code>]

"use strict";

require("sbbsdefs.js", "FL_ULTIME");
require("userdefs.js", "USER_EXTDESC");
require("attrdefs.js", "ATTR_FILENAME");
require("cga_defs.js", "HIGH");
require("text.js", "BoxHdrLib");
require("key_defs.js", "KEY_UP");
require("select_item.js", "select_item");
load("listfiles_lib.js");

function opt_attr(opts, key, color_index)
{
	if (opts[key])
		return opts[key];
	return listfiles_attr_str(console.color_list[color_index]);
}

// The options for a directory, merged from the global, library and
// directory sections
function dir_options(dir)
{
	function section(name)
	{
		try {
			return load({}, "modopts.js", name);
		} catch (e) {
			return null;
		}
	}
	return listfiles_options(listfiles_defaults()
		, section("listfiles")
		, section("listfiles:lib" + (dir.lib_number + 1))
		, section("listfiles:" + dir.code.toLowerCase()));
}

// Context of one directory's listing
function Listing(dir, mode, filespec)
{
	this.dir = dir;
	this.code = dir.code;
	this.lib = file_area.lib_list[dir.lib_index];
	this.mode = mode;
	// As the built-in lister does, a long filespec without wildcards is
	// widened before anything else (so it lists every match, with the
	// full header)
	this.filespec = (mode & FL_FIND) ? filespec : listfiles_widen_filespec(filespec);
	this.opts = null;	// loaded once there are files to list (a scan visits many without)
	this.batch = (user.settings & USER_BATCHFLAG) && !(mode & (FL_EXT | FL_VIEW));
	this.files = [];
	this.total = 0;		// files in the directory (or new), before any search
	this.longest = 0;	// longest filename
}

// Load the files to list: false if there are none or the search expression
// is invalid
Listing.prototype.load = function ()
{
	var since = 0;
	if (this.mode & FL_FIND) {
		if (!this.filespec)	// as the built-in lister does: nothing to find, nothing displayed
			return false;
		try {
			bool_search(this.filespec, "");
		} catch (e) {
			console.print(format(bbs.text(InvalidSearchExpression), e.message || String(e)));
			return false;
		}
	}
	if (this.mode & FL_ULTIME) {
		bbs.last_new_file_time = time();
		since = bbs.new_file_time;
	}
	var fb = new FileBase(this.code);
	if (!fb.open())
		return false;
	var spec = (this.mode & FL_FIND) ? "" : this.filespec;
	var list = fb.get_list(spec || "*", FileBase.DETAIL.EXTENDED, since, /* sort: */ true);
	fb.close();
	if (!list || !list.length)
		return false;
	this.total = list.length;
	for (var i = 0; i < list.length; i++) {
		var f = list[i];
		if (f.name.length > this.longest)
			this.longest = f.name.length;
		if ((this.mode & FL_FIND)
			&& !bool_search(this.filespec, f.name, f.desc || "", f.extdesc || "", f.tags || ""
			, f.author || "", f.author_org || ""))
			continue;
		this.files.push(f);
	}
	if (!this.files.length)
		return false;
	if (!this.opts)
		this.opts = dir_options(this.dir);
	return true;
};

// Whether the full (not short) directory header applies
Listing.prototype.full_header = function ()
{
	return (!this.mode || !(user.settings & USER_EXPERT))
		&& (!this.filespec || /[*?]/.test(this.filespec));
};

// Call fn() with the listed directory as the user's current one, so that
// the @-codes of a header or footer file (@LIB@, @DIR@, @FILES@, etc.)
// describe it, even in a scan of other directories
Listing.prototype.as_current_dir = function (fn)
{
	var lib0 = bbs.curlib;
	var dir0 = bbs.curdir;
	bbs.curlib = this.dir.lib_index;
	var dir_of_lib = bbs.curdir;
	bbs.curdir = this.dir.index;
	try {
		return fn();
	} finally {
		bbs.curdir = dir_of_lib;
		bbs.curlib = lib0;
		bbs.curdir = dir0;
	}
};

// Display the first menu file that exists (of the given candidates)
function show_menu(candidates)
{
	for (var i = 0; i < candidates.length; i++) {
		if (bbs.menu_exists(candidates[i])) {
			bbs.menu(candidates[i]);
			return true;
		}
	}
	return false;
}

// The directory header, followed in the interactive mode by its command keys
// (unless the header was the command keys)
Listing.prototype.show_header = function ()
{
	this.bar_row = undefined;
	if (!this.print_header() && this.interactive)
		this.print_commands();
};

// Print the interactive mode's command keys, noting their row so they (and
// their mouse hotspots) can be redrawn
Listing.prototype.print_commands = function ()
{
	this.bar_row = console.current_row;
	console.print(this.opts.interactive_commands);
};

// Print the directory header: true if it was (only) the command keys
Listing.prototype.print_header = function ()
{
	var opts = this.opts;
	var libnum = this.dir.lib_index + 1;
	var dirnum = this.dir.index + 1;
	var hdr;

	if (!this.full_header()) {
		if (this.interactive)	// the prompt names the library and directory
			return false;
		console.clearline();
		console.cond_blankline();
		hdr = format(bbs.text(ShortHdrLib), libnum, this.lib.name);
		console.print(hdr);
		var len = console.strlen(hdr);
		hdr = format(bbs.text(ShortHdrDir), dirnum, this.dir.description);
		console.print(hdr);
		len += console.strlen(hdr);
		console.newline();
		console.print(opt_attr(opts, "hdr_line_attr", ATTR_FILELSTLINE) + repeat(opts.short_hdr_underline, len));
		console.newline();
		return false;
	}
	var path = file_getcase(this.dir.data_dir + this.code + ".hdr");
	if (path) {
		this.as_current_dir(function () { console.printfile(path); });
		return false;
	}
	var candidates = listfiles_menu_candidates(this.code, this.dir.lib_number + 1, "head");
	if (this.as_current_dir(function () { return show_menu(candidates); }))
		return false;
	if (user.settings & USER_BATCHFLAG) {
		if (this.interactive)
			this.print_commands();
		else
			console.print(this.batch_commands());
		return true;
	}
	console.clear();
	var width = Math.max(this.lib.description.length, this.dir.description.length) + 17;
	if (libnum > 9 || dirnum > 9)
		width++;
	var box = opt_attr(opts, "hdr_box_attr", ATTR_FILELSTHDRBOX);
	var lines = [
		format(bbs.text(BoxHdrLib), libnum, this.lib.description),
		format(bbs.text(BoxHdrDir), dirnum, this.dir.description),
		format(bbs.text(BoxHdrFiles), this.total)
	];
	var out = box + opts.box_top_left + repeat(opts.box_horizontal, width + 1) + opts.box_top_right + "\r\n";
	for (var i = 0; i < lines.length; i++) {
		var pad = width - console.strlen(lines[i]);
		out += opts.box_vertical + " " + lines[i] + (pad > 0 ? format("%*s", pad, "") : "")
			+ box + opts.box_vertical + "\r\n";
	}
	out += opts.box_bottom_left + repeat(opts.box_horizontal, width + 1) + opts.box_bottom_right + "\r\n";
	console.print(out);
	return false;
};

// The batch flagging command keys (the interactive mode's adds Space to tag)
Listing.prototype.batch_commands = function ()
{
	if (!this.interactive)
		return bbs.text(FileListBatchCommands);
	return this.opts.interactive_commands;
};

Listing.prototype.show_tail = function ()
{
	var candidates = listfiles_menu_candidates(this.code, this.dir.lib_number + 1, "tail");
	this.as_current_dir(function () { show_menu(candidates); });
};

function repeat(str, count)
{
	var result = "";
	for (var i = 0; i < count; i++)
		result += str;
	return result;
}

// The extended description to display under/beside a file's line, or null
Listing.prototype.extdesc = function (f)
{
	if (!(user.settings & USER_EXTDESC) || !f.extdesc)
		return null;
	if (strip_ctrl(f.extdesc).replace(/\s+$/, "") === "")
		return null;
	return f.extdesc;
};

// One file's line, as a string of text and Ctrl-A codes, from the template
// 'fmt' (default: line_fmt or line_fmt_nobatch), with optional overrides of
// the module-local code values (e.g. { tag: "*" })
Listing.prototype.file_line = function (f, letter, fmt, overrides)
{
	var opts = this.opts;
	var settings = this.dir.settings;
	var namelen = (this.longest < 12 || console.screen_columns < 132) ? 12 : Math.min(this.longest, 64);
	var name_attr_num = console.color_list[ATTR_FILENAME];
	var size_attr_num = console.color_list[ATTR_FILECDT];
	var desc_attr_num = console.color_list[ATTR_FILEDESC];
	var exists = (f.size !== -1);
	var cost = f.cost || 0;
	if (!exists)
		size_attr_num = console.color_list[ATTR_ERROR];
	else if ((settings & (DIR_FREE | DIR_FCHK)) == (DIR_FREE | DIR_FCHK))
		cost = f.size;

	var values = {
		name: listfiles_format_filename(f.name, namelen),
		name_attr: opt_attr(opts, "name_attr", ATTR_FILENAME),
		flags: (f.extdesc && !(user.settings & USER_EXTDESC)) ? opts.extdesc_marker : " ",
		letter: letter || "",
		letter_attr: opts.letter_attr || listfiles_attr_str(name_attr_num ^ HIGH),
		size_attr: exists ? opt_attr(opts, "size_attr", ATTR_FILECDT) : listfiles_attr_str(size_attr_num),
		size: listfiles_size_str(cost),
		missing: exists ? " " : opts.missing_marker,
		desc_attr: opt_attr(opts, "desc_attr", ATTR_FILEDESC),
		desc: ""
	};
	if (!cost && !(settings & DIR_FREE)) {
		values.size = bbs.text(FREE);
		values.size_attr = opts.free_attr || listfiles_attr_str(size_attr_num ^ (HIGH | BLINK));
	}
	if (this.extdesc(f) === null) {
		var desc = (f.desc || "").replace(/^\s+/, "");
		if (desc === "")
			values.desc = f.name;
		else if ((this.mode & FL_FIND) && this.filespec) {
			var pos = desc.toLowerCase().indexOf(this.filespec.toLowerCase());
			if (pos >= 0)
				desc = desc.substr(0, pos)
					+ (opts.desc_attr ? opts.desc_attr + "\x01h" : listfiles_attr_str(desc_attr_num ^ HIGH))
					+ desc.substr(pos, this.filespec.length)
					+ values.desc_attr
					+ desc.substr(pos + this.filespec.length);
			values.desc = desc;
		} else
			values.desc = desc;
	}
	for (var key in overrides)
		values[key] = overrides[key];
	var code = this.code;
	if (!fmt)
		fmt = this.batch ? opts.line_fmt : opts.line_fmt_nobatch;
	return listfiles_expand(fmt, values, function (text) {
		return bbs.expand_atcodes(text, f, code);
	});
};

// Display one file: its line (or 'text', if specified) and, beside it, any
// extended description, clipped to 'max_lines' screen lines if specified
Listing.prototype.show_file = function (f, letter, text, max_lines)
{
	console.cond_newline();
	console.print(text === undefined ? this.file_line(f, letter) : text, P_TRUNCATE);
	var ext = this.extdesc(f);
	if (ext === null) {
		console.newline();
		return;
	}
	print_extdesc(ext, max_lines);
};

// Display an extended description beside (indented to) the cursor column,
// clipped to 'max_lines' lines if specified. Returns the lines displayed.
function print_extdesc(ext, max_lines)
{
	var lines = listfiles_extdesc_lines(ext);
	if (max_lines !== undefined && lines.length > max_lines)
		lines.length = Math.max(1, max_lines);
	console.putmsg(lines.join("\r\n"), P_INDENT | P_NOATCODES | P_CPM_EOF | P_TRUNCATE | P_AUTO_UTF8);
	return lines.length;
}

function aborted()
{
	return console.aborted || (bbs.sys_status & SS_ABORT);
}

// The plain (scrolling) listing, and the extended information (FL_EXT) and
// file contents (FL_VIEW) modes. Returns the number of files listed, or -1.
Listing.prototype.list = function ()
{
	if (this.batch) {
		var listed = cursor_capable() ? this.list_pages() : this.list_letters();
		if (listed > 0)
			this.show_tail();
		return listed;
	}
	var single = this.filespec && !/[*?]/.test(this.filespec) && !(this.mode & FL_FIND);
	var found = 0;
	for (var i = 0; i < this.files.length; i++) {
		if (aborted())
			return -1;
		var f = this.files[i];
		if (this.mode & (FL_EXT | FL_VIEW)) {
			if (!found)
				console.print("\x01[\x01>");
			// As listfileinfo() does: no newline after each file (the next
			// file's screen is cleared without a pause), and Previous goes
			// back a file
			var result = bbs.view_file_info(this.code, f.name, (this.mode & FL_EXT) ? true : false);
			if (result === 0)
				return -1;
			// Previous goes back a file: count each file once
			found = Math.max(found, i + 1);
			if (result === -2)
				i = Math.max(i - 1, 0) - 1;
		} else {
			if (!found)
				this.show_header();
			this.show_file(f);
			found++;
		}
		if (aborted())
			return -1;
		if (single)
			break;
	}
	if (found && !(this.mode & (FL_EXT | FL_VIEW)))
		this.show_tail();
	return found;
};

// Batch flagging: result codes of a command, as in the built-in lister
var CMD_QUIT = -1;
var CMD_SKIP = 0;		// skip the rest of the directory
var CMD_NEXT = 1;
var CMD_REDRAW = 2;
var CMD_PREV = 3;

Listing.prototype.can_remove = function ()
{
	return !(user.security.restrictions & UFLAG_R)
		&& (this.dir.is_operator || (user.security.exemptions & UFLAG_R));
};

// Choose a library and directory to move files to: a directory code, or null
Listing.prototype.choose_dest = function ()
{
	var libs = file_area.lib_list;
	function item(fmt, num, desc)
	{
		return format(bbs.text(fmt), num, desc).replace(/\r\n$/, "");
	}
	var lib = select_item({
		count: libs.length,
		current: this.dir.lib_index,
		prompt: function (num) { return format(bbs.text(MoveToLibPrompt), num); },
		print_item: function (i) { console.print(item(MoveToLibLstFmt, i + 1, libs[i].description)); }
	});
	if (lib < 0)
		return null;
	var dirs = libs[lib].dir_list;
	var dir = select_item({
		count: dirs.length,
		current: dirs.length - 1,
		prompt: function (num) { return format(bbs.text(MoveToDirPrompt), num); },
		print_item: function (i) { console.print(item(MoveToDirLstFmt, i + 1, dirs[i].description)); }
	});
	if (dir < 0)
		return null;
	return dirs[dir].code;
};

// Act on a batch flagging command key. 'choose' is a function returning the
// files to act on (an array), or null to prompt again.
// Returns a CMD_* value, or null to prompt again.
Listing.prototype.command = function (ch, choose)
{
	var code = this.code;
	var files, i;

	if (ch === "?") {
		if (!this.interactive || !bbs.menu("listfiles_help", P_NOERROR))
			bbs.menu("batflag");
		if (console.line_counter)
			console.pause();
		return CMD_REDRAW;
	}
	if (ch === console.quit_key || aborted())
		return CMD_QUIT;
	switch (ch) {
		case "S":
			return CMD_SKIP;
		case "P":
		case "-":
			return CMD_PREV;
		case "T":
			user.settings ^= USER_EXTDESC;
			return CMD_REDRAW;
		case "B":
		case "D":
			if (user.security.restrictions & UFLAG_D) {
				console.print(bbs.text(R_Download));
				return CMD_REDRAW;
			}
			if ((files = choose()) === null)
				return null;
			for (i = 0; i < files.length; i++) {
				if (!bbs.batch_add_file(code, files[i].name))
					this.report_vanished(files[i]);
			}
			if (ch === "D") {
				bbs.batch_download();
				if (console.line_counter)
					console.pause();
			}
			console.newline();
			return CMD_REDRAW;
		case "E":
		case "V":
			if ((files = choose()) === null)
				return null;
			for (i = 0; i < files.length; i++) {
				if (bbs.view_file_info(code, files[i].name, ch === "E") === 0)
					return CMD_QUIT;
			}
			console.cond_newline();
			return CMD_REDRAW;
		case "R":
		case "M":
			if (!this.can_remove())
				return CMD_NEXT;
			if ((files = choose()) === null)
				return null;
			var remfile = true;
			var remcdt = true;
			var dest = null;
			if (ch === "R") {
				if (console.noyes(bbs.text(RemoveFileQ)))
					return CMD_REDRAW;
				if (this.dir.is_operator) {
					remfile = !console.noyes(bbs.text(DeleteFileQ));
					remcdt = !console.noyes(bbs.text(RemoveCreditsQ));
				}
			} else if ((dest = this.choose_dest()) === null)
				return CMD_REDRAW;
			for (i = 0; i < files.length; i++) {
				var done = (ch === "R")
					? bbs.remove_file(code, files[i].name, remfile, remcdt)
					: bbs.move_file(code, files[i].name, dest);
				if (!done)
					this.report_vanished(files[i]);
			}
			this.reload();
			return CMD_REDRAW;
	}
	return CMD_NEXT;
};

// After an action on file f failed: if f is no longer in the directory
// (e.g. another node removed it since it was listed), say so; any other
// failure has already been reported by the action itself
Listing.prototype.report_vanished = function (f)
{
	var fb = new FileBase(this.code);
	if (!fb.open())
		return;
	var listed = fb.get(f.name) !== null;
	fb.close();
	if (!listed)
		console.print(format(bbs.text(FileDoesNotExist), f.name));
};

// Re-read the directory after files were removed or moved
Listing.prototype.reload = function ()
{
	this.files = [];
	this.longest = 0;
	this.load();
};

// The batch flagging prompt (with the number of files on this screen)
Listing.prototype.prompt = function (count, in_place)
{
	if (!in_place)
		console.cond_blankline();
	console.print(format(bbs.text(BatchFlagPrompt)
		, this.dir.lib_index + 1, this.lib.name
		, this.dir.index + 1, this.dir.name
		, count, this.files.length));
};

// Letters, filenames or filespecs typed to pick files of 'page' to act on
function letter_chooser(page)
{
	return function () {
		if (page.length === 1)
			return page;
		console.print(bbs.text(BatchDlFlags));
		var str = console.getstr(26, K_NOCRLF | K_UPPER);
		if (!str) {
			console.clearline();
			return null;
		}
		console.newline();
		var chosen = [];
		for (var c = 0; c < str.length; c++) {
			var rest = str.substr(c);
			if (rest.charAt(0) === "*" || rest.indexOf(".") >= 0) {
				var spec = rest.split(/[ ,]/)[0];
				for (var i = 0; i < page.length; i++) {
					if (wildmatch(page[i].name, spec))
						chosen.push(page[i]);
				}
				break;
			}
			var idx = str.charCodeAt(c) - 65;
			if (idx >= 0 && idx < page.length)
				chosen.push(page[idx]);
		}
		return chosen;
	};
}

// The built-in lister's batch flagging flow: up to 26 files (A-Z) per
// screenful, each followed by the batch flagging prompt
Listing.prototype.list_letters = function ()
{
	var single = this.filespec && !/[*?]/.test(this.filespec) && !(this.mode & FL_FIND);
	var rows = console.screen_rows;
	var found = 0;
	var anchor = 0;		// index of the screen's first file
	var i = 0;
	var page = [];

	while (bbs.online) {
		if (i >= this.files.length && !page.length)
			break;
		if (aborted())
			return -1;
		var full = false;
		if (i < this.files.length) {
			var f = this.files[i];
			if (!found)
				this.show_header();
			else if (!page.length)
				console.print(bbs.text(FileListBatchCommands));
			var ext = this.extdesc(f);
			if (ext !== null && page.length
				&& console.line_counter + listfiles_extdesc_lines(ext).length >= rows - 2) {
				full = true;
			} else {
				if (!page.length)
					anchor = i;
				this.show_file(f, String.fromCharCode(65 + page.length));
				page.push(f);
				found++;
				i++;
				full = single || (page.length === 26) || (console.line_counter >= rows - 2);
			}
		} else
			full = true;
		if (!full)
			continue;
		console.line_counter = 0;
		var result = null;
		while (result === null && bbs.online) {
			this.prompt(page.length);
			var ch = console.getkey(K_UPPER);
			console.clearline();
			result = this.command(ch, letter_chooser(page));
		}
		switch (result) {
			case CMD_QUIT:
				return -1;
			case CMD_SKIP:
				return found;
			case CMD_REDRAW:
				i = Math.min(anchor, this.files.length);
				found = i;	// the files before this screen's
				break;
			case CMD_PREV:
				i = Math.max(0, anchor - page.length);
				found = i;
				break;
			default:
				if (single || i >= this.files.length)
					return found;
		}
		page = [];
	}
	return found;
};

// Whether the terminal can position the cursor (for the interactive mode)
function cursor_capable()
{
	return console.term_supports(USER_ANSI) || console.term_supports(USER_PETSCII);
}

// Sent by a mouse click on a file's line, followed by a character for the
// file's position on the page (A for the first): a key no command or
// navigation key uses
var HOTSPOT_PREFIX = "\x1f";

// A second click on the same file within this many seconds is a double-click
// (which tags or untags the file)
var DOUBLE_CLICK_SECONDS = 0.5;

// Interactive batch flagging: a highlighted current file moved with the
// arrow keys (scrolling as needed) and paged with PgUp/PgDn, Space or a
// double-click to tag files, and mouse selection of files
Listing.prototype.list_pages = function ()
{
	var opts = this.opts;
	var cur = 0;			// index of the current file
	var tagged = {};		// tagged filenames
	var last_click = null;	// { file: index, time: system.timer } of the last click
	var listing = this;
	var top = 0;			// index of the first file on the screen
	var heights;			// screen lines of each file
	var rows;				// screen lines for files
	var list_row;			// screen row of the first file
	var shown_prompt = null;	// the prompt as displayed (with its hotspots)
	var prompt_end;			// screen column after the displayed prompt

	function measure()
	{
		heights = [];
		for (var i = 0; i < listing.files.length; i++) {
			var ext = listing.extdesc(listing.files[i]);
			heights.push(ext === null ? 1 : listfiles_extdesc_lines(ext).length);
		}
	}

	// The number of files on the screen, and the index of the last one
	function visible()
	{
		return listfiles_window_count(heights, top, rows);
	}
	function bottom()
	{
		return top + visible() - 1;
	}

	// File i's line, highlighted when it's the current file
	function line(i)
	{
		var f = listing.files[i];
		var overrides = { tag: tagged[f.name] ? opts.tagged_marker : opts.untagged_marker };
		if (i === cur && !opts.lightbar)
			overrides.name_attr = opts.selected_item_attr;
		var text = listing.file_line(f, "", opts.line_fmt_interactive, overrides);
		if (i === cur && opts.lightbar)
			text = "\x01n" + opts.selected_item_attr + strip_ctrl(text);
		return text;
	}

	// Draw file i at screen row 'row' (0-based): its line and any extended
	// description beside it, at most 'max_lines' lines, each cleared to its
	// end. Returns the number of lines drawn.
	function draw_entry(i, row, max_lines)
	{
		console.gotoxy(1, row + 1);
		console.print(line(i), P_TRUNCATE);
		var lines = 1;
		var ext = listing.extdesc(listing.files[i]);
		if (ext !== null) {
			var col = console.current_column;
			var ext_lines = listfiles_extdesc_lines(ext);
			if (ext_lines.length > max_lines)
				ext_lines.length = Math.max(1, max_lines);
			for (var k = 0; k < ext_lines.length; k++) {
				if (k) {	// blank the line up to the description column too
					clear_to_eol();
					console.gotoxy(1, row + k + 1);
					var attr = console.attributes;
					console.print("\x01n" + format("%*s", col, ""));
					console.attributes = attr;
				}
				console.putmsg(ext_lines[k], P_NOATCODES | P_CPM_EOF | P_TRUNCATE | P_AUTO_UTF8);
			}
			lines = ext_lines.length;
		}
		clear_to_eol();
		return lines;
	}

	// Clear the rest of the line in the normal attribute, keeping the
	// current one for what follows
	function clear_to_eol()
	{
		var attr = console.attributes;
		console.print("\x01n");
		console.cleartoeol();
		console.attributes = attr;
	}

	// Redraw file i in place
	function draw(i)
	{
		var k = i - top;
		draw_entry(i, listing.file_rows[k], listing.file_heights[k]);
	}

	// Draw the files on the screen: 'full' to clear the screen and display
	// the header first, otherwise (scrolling) each file's lines are redrawn
	// in place, without the flash of a cleared screen
	function draw_page(full)
	{
		console.clear_hotspots();
		shown_prompt = null;	// its hotspots are gone too
		if (full || list_row === undefined) {
			console.line_counter = 0;	// as the built-in's "\x01q" does: clear without a pause
			console.clear();
			listing.show_header();
			console.cond_newline();
			list_row = console.current_row;
			if (rows === undefined) {
				rows = Math.max(1, console.screen_rows - list_row - 2);	// a blank line, then the prompt
				measure();
			}
			full = true;
		} else if (listing.bar_row !== undefined && opts.interactive_commands) {
			// clearing the hotspots dropped the command keys' too
			console.gotoxy(1, listing.bar_row + 1);
			console.print(opts.interactive_commands);
		}
		if (!listing.files.length) {	// all removed or moved: leave() from here
			listing.list_end_row = list_row;
			listing.prompt_row = list_row;
			return;
		}
		if (cur < top)
			top = cur;
		else if (cur > bottom())
			top = listfiles_window_top(heights, cur, rows);
		listing.file_rows = [];
		listing.file_heights = [];
		var row = list_row;
		for (var i = top; i <= bottom(); i++) {
			var drawn = draw_entry(i, row, rows - (row - list_row));
			listing.file_rows.push(row);
			listing.file_heights.push(drawn);
			console.add_hotspot(HOTSPOT_PREFIX + String.fromCharCode(65 + i - top), false
				, 0, console.screen_columns - 1, row);
			row += drawn;
		}
		if (!full) {	// what's left of a longer list, and its prompt
			for (var r = row; r <= listing.prompt_row; r++) {
				console.gotoxy(1, r + 1);
				console.cleartoeol();
			}
		}
		listing.list_end_row = row;
		listing.prompt_row = Math.min(row + 1, console.screen_rows - 1);
		console.line_counter = 0;
	}

	// The current file's position, right-justified on the (blank) line
	// above the prompt
	function draw_position()
	{
		var row = listing.prompt_row - 1;
		if (!opts.position_fmt || row < listing.list_end_row)
			return;
		var text = format(opts.position_fmt, cur + 1, listing.files.length);
		var col = console.screen_columns - console.strlen(text);	// clear of the last column
		console.gotoxy(1, row + 1);
		console.cleartoeol();
		if (col > 0) {
			console.gotoxy(col, row + 1);
			console.print(text + "\x01n");
		}
	}

	function toggle_tag(i)
	{
		var name = listing.files[i].name;
		if (tagged[name])
			delete tagged[name];
		else
			tagged[name] = true;
		draw(i);
	}

	// Done with this directory: clear the position and prompt lines, leaving
	// the cursor where they were (for a footer, or the next directory)
	function leave()
	{
		console.clear_hotspots();
		var row = Math.max(listing.list_end_row, listing.prompt_row - 1);
		for (var r = row; r <= listing.prompt_row; r++) {
			console.gotoxy(1, r + 1);
			console.cleartoeol();
		}
		console.gotoxy(1, row + 1);
	}

	// The files to act on: the tagged ones, if any, otherwise the current one
	function chooser()
	{
		var chosen = [];
		for (var i = 0; i < listing.files.length; i++) {
			if (tagged[listing.files[i].name])
				chosen.push(listing.files[i]);
		}
		if (!chosen.length)
			chosen.push(listing.files[cur]);
		return chosen;
	}

	// Make file 'to' the current one, scrolling (redrawing) only as far as
	// needed to show it
	function move(to)
	{
		to = Math.max(0, Math.min(listing.files.length - 1, to));
		if (to === cur)
			return;
		var old = cur;
		cur = to;
		if (cur < top || cur > bottom())
			draw_page(/* full: */ false);
		else {
			draw(old);
			draw(cur);
		}
	}

	// Show the next/previous screenful of files, the first one current
	function page_down()
	{
		if (bottom() >= listing.files.length - 1) {
			move(listing.files.length - 1);
			return;
		}
		top = bottom() + 1;
		cur = top;
		draw_page(/* full: */ false);
	}
	function page_up()
	{
		if (top === 0) {
			move(0);
			return;
		}
		top = listfiles_window_top(heights, top - 1, rows);
		cur = top;
		draw_page(/* full: */ false);
	}

	this.interactive = true;
	var total = this.files.length;	// the files listed, including any removed
	draw_page(/* full: */ true);
	while (bbs.online) {
		if (!this.files.length) {
			leave();
			return total;
		}
		draw_position();
		var up = top > 0 ? opts.more_above : opts.no_more;
		var down = bottom() < this.files.length - 1 ? opts.more_below : opts.no_more;
		// Long library/directory names are shortened so the end of the prompt
		// fits; the prompt is still truncated, never wrapped (which would
		// scroll the screen), if even that isn't enough
		var prompt = listfiles_shorten_to_fit([this.lib.name, this.dir.name]
			, function (names) {
				return format(opts.interactive_prompt, up, down
					, listing.dir.lib_index + 1, names[0]
					, listing.dir.index + 1, names[1]
					, listing.files.length);
			}
			, function (text) { return console.strlen(text); }
			, console.screen_columns - 1);
		// Print the prompt (adding its mouse hotspots) only when it changed or
		// the hotspots were cleared; otherwise just return to its end
		if (prompt !== shown_prompt) {
			console.gotoxy(1, this.prompt_row + 1);
			console.cleartoeol();
			console.print(prompt, P_TRUNCATE);
			shown_prompt = prompt;
			prompt_end = console.current_column;
		} else
			console.gotoxy(prompt_end + 1, this.prompt_row + 1);
		var ch = console.getkey(K_UPPER | K_NOCRLF);
		if (aborted()) {
			leave();
			return -1;
		}
		if (ch === HOTSPOT_PREFIX) {	// a mouse click on a file
			var idx = console.getkey().charCodeAt(0) - 65;
			if (idx >= 0 && idx < visible()) {
				var clicked = top + idx;
				var now = system.timer;
				if (last_click && last_click.file === clicked
					&& now - last_click.time < DOUBLE_CLICK_SECONDS) {
					toggle_tag(clicked);
					last_click = null;
				} else {
					move(clicked);
					last_click = { file: clicked, time: now };
				}
			}
			continue;
		}
		last_click = null;
		switch (ch) {
			case KEY_UP:
				move(cur - 1);
				continue;
			case KEY_DOWN:
				move(cur + 1);
				continue;
			case KEY_PAGEUP:
			case "\b":
				page_up();
				continue;
			case KEY_PAGEDN:
				page_down();
				continue;
			case KEY_HOME:
				move(0);
				continue;
			case KEY_END:
				move(this.files.length - 1);
				continue;
			case " ":
				toggle_tag(cur);
				move(cur + 1);	// as in msglist.js
				continue;
		}
		if (ch === "\r" || ch === "N") {
			if (bottom() >= this.files.length - 1) {
				leave();
				return total;
			}
			page_down();
			continue;
		}
		if (ch === KEY_DEL)	// as in msglist.js
			ch = "R";
		console.clear_hotspots();
		console.clearline();
		shown_prompt = null;	// cleared along with its hotspots
		var result = this.command(ch, chooser);
		if (result === null)
			continue;
		switch (result) {
			case CMD_QUIT:
				if (ch !== "E" && ch !== "V")	// (their file info replaced the list)
					leave();
				return -1;
			case CMD_SKIP:
				leave();
				return total;
			case CMD_PREV:
				if (top > 0) {
					top = listfiles_window_top(heights, top - 1, rows);
					cur = top;
				}
				break;
			case CMD_NEXT:	// nothing done: restore the hotspots cleared above
				draw_page(/* full: */ false);
				continue;
		}
		if (result === CMD_REDRAW && (ch === "B" || ch === "D"))
			tagged = {};
		rows = undefined;	// files or extended descriptions may have changed
		cur = Math.max(0, Math.min(cur, this.files.length - 1));
		top = Math.min(top, cur);
		draw_page(/* full: */ true);
	}
	return -1;
};

function main(args)
{
	var code = String(args[0] || "").toLowerCase();
	if (!code) {	// run with no arguments (e.g. ;EXEC ?listfiles): the current directory
		var lib = file_area.lib_list[bbs.curlib];
		if (!lib || !lib.dir_list[bbs.curdir])
			return 0;
		code = lib.dir_list[bbs.curdir].code;
	}
	var dir = file_area.dir[code];
	if (!dir || dir.lib_index < 0 || dir.index < 0)	// not accessible to this user
		return 0;
	var mode = parseInt(args[1], 10) || 0;
	var filespec = args.slice(2).join(" ");
	var listing = new Listing(dir, mode, filespec);
	if (!listing.load())
		return 0;
	return listing.list();
}

exit(main(argv));
