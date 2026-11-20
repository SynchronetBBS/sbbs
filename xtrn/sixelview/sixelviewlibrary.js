/* sixelviewlibrary.js
 *
 * Library browser for SixelView.  Lists the libraries (directories of PDF, image,
 * ANSI/ASCII art and text files) configured in settings.ini, filtered by each library's
 * ARS, include and exclude settings, and opens the chosen file in the SixelView viewer
 * (PDF and images) or the text/ANSI viewer.  ZIP files in libraries that allow
 * sub-directories are extracted on demand into a directory of the same name.
 * S and P in the lists set the output speed and screen pausing for text/ANSI files.
 *
 *	?sixelviewlibrary [force]
 *
 * Configuration: settings.ini (see settings-example.ini) and the theme file it names.
 * Requires sixelview_lib.js.  Tab size is 4.
 */

"use strict";

load("sbbsdefs.js");
load("key_defs.js");
load("sixelview_lib.js");

var IMAGE_TYPES = ["pdf", "png", "jpg", "jpeg", "gif", "bmp", "ppm", "pgm", "six"];
var TEXT_TYPES = ["ans", "ansi", "asc", "txt", "msg", "diz", "nfo", "bbs"];
var SUPPORTED_TYPES = IMAGE_TYPES.concat(TEXT_TYPES, ["zip"]);

/* Output speeds for text/ANSI files; index 0 = unlimited (the index is also what SyncTERM is sent) */
var SPEEDS = [0, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200];
var opt = { speed: 0, pause: false, zip_max_bytes: 50 * 1048576, zip_max_files: 1000,
	image_max_bytes: 3 * 1048576, cache_max_days: 14, cache_max_bytes: 100 * 1048576 };
var syncterm;

var BASE_DIR = js.exec_dir;

/* Fallback colours/text, used for anything the theme file does not set */
var DEFAULT_THEME = {
	headerTextColor: "n7r",
	headerNameColor: "b",
	colHeaderColor: "nhw",
	listNumColor: "nhy",
	listNameColor: "nc",
	listDescColor: "nc",
	listTypeColor: "ng",
	listSizeColor: "nc",
	listDateColor: "hb",
	listDirColor: "nhy",
	listHighlightBkgColor: "4",
	listNumHighlightColor: "hy",
	listNameHighlightColor: "hc",
	listDescHighlightColor: "hc",
	listTypeHighlightColor: "hc",
	listSizeHighlightColor: "hw",
	listDateHighlightColor: "hw",
	listDirHighlightColor: "hy",
	helpLineBkgColor: "7",
	helpLineGeneralColor: "b",
	helpLineHotkeyColor: "r",
	helpLineParenColor: "m",
	textBodyColor: "nw",
	scrollbarBGColor: "nhk",
	scrollbarBGChar: "\\xb0",
	scrollbarScrollBlockColor: "nhw",
	scrollbarScrollBlockChar: "\\xdb",
	titleText: "\\x01n\\x01h\\x01wSixelView Library",
	noLibrariesText: "\\x01n\\x01h\\x01yThere are no libraries available.",
	emptyLibraryText: "\\x01n\\x01h\\x01yThere are no files to display here.",
	errorText: "\\x01n\\x01h\\x01rUnable to display file: \\x01w%s",
	loadingText: "\\x01n\\x01h\\x01cLoading...",
	hookErrorText: "\\x01n\\x01h\\x01rUnable to get the list from the server."
};

var theme = {};

/* ---------- Helpers ---------- */

function is_absolute(path)
{
	return /^([a-zA-Z]:)?[\\\/]/.test(path);
}

function resolve_path(path)
{
	return is_absolute(path) ? path : BASE_DIR + path;
}

/* Comma-separated wildcard patterns (* and ?, case-insensitive) -> array of RegExp.
 * A bare extension such as "pdf" or ".pdf" is accepted as *.pdf */
function split_list(str)
{
	var out = [];
	if (!str)
		return out;
	var parts = String(str).split(/\s*[,;]\s*|\s+/);
	for (var i = 0; i < parts.length; i++) {
		var p = parts[i];
		if (!p.length)
			continue;
		if (!/[*?]/.test(p))
			p = "*." + p.replace(/^\./, "");
		var re = p.replace(/[\\^$.+()\[\]{}|\/]/g, "\\$&").replace(/\*/g, ".*").replace(/\?/g, ".");
		out.push(new RegExp("^" + re + "$", "i"));
	}
	return out;
}

function matches_any(list, name)
{
	for (var i = 0; i < list.length; i++) {
		if (list[i].test(name))
			return true;
	}
	return false;
}

/* Turn colour codes ("nhy", "n7r") into Ctrl-A attribute characters */
function attr(codes)
{
	var s = "";
	codes = String(codes || "");
	for (var i = 0; i < codes.length; i++)
		s += "\x01" + codes.charAt(i);
	return s;
}

/* Expand \xNN escapes in theme values */
function unescape_text(str)
{
	return String(str).replace(/\\x([0-9a-fA-F]{2})/g, function (m, h) {
		return ascii(parseInt(h, 16));
	});
}

function T(name)
{
	return theme[name];
}

function C(name)
{
	return attr(theme[name]);
}

/* Highlighted colour: the highlight foreground with the highlight background */
function CH(name)
{
	return attr(theme[name]) + "\x01" + theme.listHighlightBkgColor.charAt(0);
}

function fit(str, width)
{
	str = String(str);
	if (str.length > width)
		return str.substr(0, width);
	while (str.length < width)
		str += " ";
	return str;
}

function rjust(str, width)
{
	str = String(str);
	if (str.length > width)
		return str.substr(0, width);
	while (str.length < width)
		str = " " + str;
	return str;
}

function format_size(n)
{
	if (n >= 1048576)
		return (n / 1048576).toFixed(1) + "M";
	if (n >= 1024)
		return Math.round(n / 1024) + "K";
	return String(n);
}

function format_date(t)
{
	return strftime("%Y-%m-%d", t);
}

function file_ext(name)
{
	var i = name.lastIndexOf(".");
	return i < 0 ? "" : name.substr(i + 1).toLowerCase();
}

function pause_error(msg)
{
	console.print("\r\n" + msg + "\x01n\r\n");
	console.pause();
}

/* ---------- Configuration ---------- */

function read_theme(filename)
{
	var key;
	for (key in DEFAULT_THEME)
		theme[key] = DEFAULT_THEME[key];
	if (!filename)
		return;
	var f = new File(resolve_path(filename));
	if (!f.open("r")) {
		log(LOG_WARNING, "sixelviewlibrary: unable to open theme " + f.name);
		return;
	}
	var obj = f.iniGetObject();
	f.close();
	for (key in obj) {
		if (DEFAULT_THEME.hasOwnProperty(key))
			theme[key] = String(obj[key]);
	}
}

function read_settings()
{
	var f = new File(BASE_DIR + "settings.ini");
	if (!f.open("r")) {
		log(LOG_ERR, "sixelviewlibrary: unable to open " + f.name);
		return null;
	}
	var root = f.iniGetObject() || {};
	var libraries = [];
	var sections = f.iniGetSections("library:");
	for (var i = 0; i < sections.length; i++) {
		var o = f.iniGetObject(sections[i]);
		if (!o || !(o.dir || o.module))
			continue;
		var sort = String(o.sort || "name").toLowerCase();
		var lib = {
			name: sections[i].substr("library:".length),
			desc: o.desc || "",
			dir: o.dir ? backslash(resolve_path(String(o.dir))) : "",
			module: o.module ? String(o.module) : "",
			options: o,
			ars: o.ars ? String(o.ars) : "",
			include: split_list(o.include),
			exclude: split_list(o.exclude).concat(split_list(o.hide)),	/* hide= is ansiview's name for exclude= */
			subdirs: !o.module && (o.subdirs === true || /^(true|yes|1)$/i.test(String(o.subdirs))),
			reverse: sort.charAt(0) == "-",
			sort: sort.replace(/^-/, "")
		};
		if (["name", "date", "size"].indexOf(lib.sort) < 0)
			lib.sort = "name";
		libraries.push(lib);
	}
	f.close();
	if (root.pause === true || /^(true|yes|1)$/i.test(String(root.pause)))
		opt.pause = true;
	if (SPEEDS.indexOf(Number(root.speed)) > 0)
		opt.speed = SPEEDS.indexOf(Number(root.speed));
	if (Number(root.zip_max_mb) > 0)
		opt.zip_max_bytes = Number(root.zip_max_mb) * 1048576;
	if (Number(root.zip_max_files) > 0)
		opt.zip_max_files = Number(root.zip_max_files);
	if (Number(root.image_max_mb) > 0)
		opt.image_max_bytes = Number(root.image_max_mb) * 1048576;
	if (root.cache_max_days !== undefined && Number(root.cache_max_days) >= 0)
		opt.cache_max_days = Number(root.cache_max_days);
	if (root.cache_max_mb !== undefined && Number(root.cache_max_mb) >= 0)
		opt.cache_max_bytes = Number(root.cache_max_mb) * 1048576;
	return { theme: root.theme, libraries: libraries };
}

/* Is a file of this type shown in this library? */
function type_allowed(lib, name)
{
	var ext = file_ext(name);
	if (SUPPORTED_TYPES.indexOf(ext) < 0)
		return false;
	if (ext == "zip" && !lib.subdirs)	/* extracted archives are browsed as sub-directories */
		return false;
	if (lib.include.length && !matches_any(lib.include, name))
		return false;
	if (matches_any(lib.exclude, name))
		return false;
	return true;
}

function library_allowed(lib)
{
	return !lib.ars.length || bbs.compare_ars(lib.ars);
}

/* ---------- Hooks (online archives) ---------- */

/*
 * A library with module=<file> instead of dir= gets its listing from a hook in the hooks
 * directory.  See hooks/sixteencolors.js for the interface.  Hook files and downloads are
 * cached under .cache/<module>/ (cache_ttl seconds for listings, default 86400).
 */
function open_hook(lib)
{
	if (lib.hook !== undefined)
		return lib.hook;
	lib.hook = null;
	var modname = lib.module.replace(/[^A-Za-z0-9_.\-]/g, "");
	var cache_dir = BASE_DIR + ".cache/" + modname.replace(/\.js$/, "") + "/";
	var ttl = Number(lib.options.cachettl) >= 0 && lib.options.cachettl !== undefined ? Number(lib.options.cachettl) : 86400;

	function safe_name(n)
	{
		return String(n).replace(/[^A-Za-z0-9._\-\/]/g, "_").replace(/\.\.+/g, "_").replace(/^\/+/, "");
	}
	function write_file(path, data)
	{
		mkpath(path.replace(/[^\/]*$/, ""));
		var f = new File(path);
		if (!f.open("wb"))
			return false;
		f.write(data);
		f.close();
		return true;
	}
	function http_get(url)
	{
		try {
			load("http.js");
			return new HTTPRequest().Get(url);
		}
		catch (e) {
			log(LOG_WARNING, "sixelviewlibrary: " + url + ": " + e);
			return null;
		}
	}
	var ctx = {
		options: lib.options,
		cache_dir: cache_dir,
		get_json: function (url, cache_name) {
			var cf = cache_dir + safe_name(cache_name);
			var cached = file_exists(cf) ? file_date(cf) : 0;
			if (!cached || time() - cached > ttl) {
				var data = http_get(url);
				if (data) {
					try {
						var obj = JSON.parse(data);
						write_file(cf, data);
						return obj;
					}
					catch (e) {}
				}
				if (!cached)
					return null;
			}
			var f = new File(cf);
			if (!f.open("r"))
				return null;
			var txt = f.read();
			f.close();
			try { return JSON.parse(txt); } catch (e2) { return null; }
		},
		get_file: function (url, local_name) {
			var path = cache_dir + "files/" + safe_name(local_name);
			if (file_exists(path)) {
				file_touch(path);	/* recently used files are kept longest by clean_cache() */
				return path;
			}
			var data = http_get(url);
			if (data === null || data === undefined || !write_file(path, data))
				return null;
			return path;
		}
	};
	try {
		var scope = load({}, BASE_DIR + "hooks/" + modname);
		lib.hook = scope.create(ctx);
	}
	catch (e) {
		log(LOG_ERR, "sixelviewlibrary: hook " + modname + ": " + e);
	}
	return lib.hook;
}

/* Delete cached files older than cache_max_days (0 = no age limit); then, if the cache is still
 * larger than cache_max_mb (0 = no size limit), delete the least recently used files until it fits. */
function clean_cache()
{
	var root = BASE_DIR + ".cache/";
	if (!file_isdir(root))
		return;
	var files = [];
	var dirs = [];
	function walk(dir)
	{
		var paths = directory(dir + "*");
		for (var i = 0; i < paths.length; i++) {
			var p = paths[i].replace(/[\\\/]+$/, "");
			if (file_isdir(p)) {
				walk(p + "/");
				dirs.push(p);
			}
			else
				files.push({ path: p, date: file_date(p), size: file_size(p) });
		}
	}
	walk(root);
	var now = time();
	var recent = 600;	/* never touch files from the last 10 minutes (another user may be using them) */
	var total = 0;
	var keep = [];
	for (var i = 0; i < files.length; i++) {
		var f = files[i];
		if (opt.cache_max_days > 0 && now - f.date > opt.cache_max_days * 86400 && now - f.date > recent)
			file_remove(f.path);
		else {
			keep.push(f);
			total += f.size;
		}
	}
	if (opt.cache_max_bytes > 0 && total > opt.cache_max_bytes) {
		keep.sort(function (a, b) { return a.date - b.date; });
		for (i = 0; i < keep.length && total > opt.cache_max_bytes; i++) {
			if (now - keep[i].date <= recent)
				break;
			if (file_remove(keep[i].path))
				total -= keep[i].size;
		}
	}
	for (i = 0; i < dirs.length; i++)	/* remove directories left empty (fails harmlessly if not empty) */
		rmdir(dirs[i]);
}

function list_hook_entries(lib, subpath)
{
	var hook = open_hook(lib);
	var items = hook ? hook.list(subpath) : null;
	if (!items)
		return null;
	var out = [];
	if (subpath.length)
		out.push({ name: "..", isdir: true, up: true, size: 0, date: 0 });
	for (var i = 0; i < items.length; i++) {
		var it = items[i];
		if (!it.isdir) {
			if (!type_allowed(lib, it.name))
				continue;
			it.type = file_ext(it.name).toUpperCase();
		}
		out.push(it);
	}
	return out;
}

/* ---------- Directory listing ---------- */

/* subpath: array of sub-directory names below the library directory */
function list_entries(lib, subpath)
{
	var dir = lib.dir + (subpath.length ? subpath.join("/") + "/" : "");
	var paths = directory(dir + "*");
	var dirs = [];
	var files = [];
	for (var i = 0; i < paths.length; i++) {
		var p = paths[i].replace(/[\\\/]+$/, "");
		var name = p.substr(Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")) + 1);
		if (!name.length)
			continue;
		if (file_isdir(p)) {
			if (lib.subdirs)
				dirs.push({ name: name, isdir: true, path: p, size: 0, date: file_date(p) });
		}
		else if (type_allowed(lib, name)) {
			/* a ZIP that has already been extracted is hidden (its directory is listed instead) */
			if (file_ext(name) == "zip" && file_isdir(zip_dest(p)))
				continue;
			files.push({ name: name, isdir: false, path: p, size: file_size(p), date: file_date(p),
				type: file_ext(name).toUpperCase(), iszip: file_ext(name) == "zip" });
		}
	}
	function cmp(a, b)
	{
		var r;
		if (lib.sort == "date")
			r = a.date - b.date;
		else if (lib.sort == "size")
			r = a.size - b.size;
		else
			r = 0;
		if (r == 0) {
			var an = a.name.toLowerCase();
			var bn = b.name.toLowerCase();
			r = an < bn ? -1 : an > bn ? 1 : 0;
			return r;
		}
		return lib.reverse ? -r : r;
	}
	dirs.sort(function (a, b) {
		var an = a.name.toLowerCase();
		var bn = b.name.toLowerCase();
		return an < bn ? -1 : an > bn ? 1 : 0;
	});
	files.sort(cmp);
	var out = [];
	if (subpath.length)
		out.push({ name: "..", isdir: true, up: true, size: 0, date: 0 });
	return out.concat(dirs, files);
}

function count_files(lib)
{
	var n = 0;
	var paths = directory(lib.dir + "*");
	for (var i = 0; i < paths.length; i++) {
		var p = paths[i].replace(/[\\\/]+$/, "");
		if (!file_isdir(p) && type_allowed(lib, file_getname(p)))
			n++;
	}
	return n;
}

/* ---------- Generic lightbar list ---------- */

/*
 * title:   header text (plain), subtitle: name shown after it
 * cols:    array of {head, width, align, color, hicolor, get: function(item)}; one column
 *          (the one with flex: true) takes the remaining width
 * items:   array of items
 * help:    array of [hotkey, text], or a function returning one
 * initial: index of the item to start on (default 0)
 * Returns { item: <selected item>, index: <its index> } or null on quit.
 * onkey(key, item) may return true to end the list with that key.
 */
function lightbar(title, subtitle, cols, items, help, onkey, initial)
{
	var W = console.screen_columns;
	var H = console.screen_rows;
	var listTop = 3;
	var listRows = H - listTop - 1;
	var sbX = W;
	var width = W - 1;		/* leave the last column for the scrollbar */
	var sel = 0;
	var top = 0;
	var i;
	if (initial > 0 && initial < items.length) {
		sel = initial;
		if (sel >= listRows)
			top = Math.max(0, Math.min(items.length - listRows, sel - Math.floor(listRows / 2)));
	}

	/* Column widths */
	var fixed = 0;
	var flex = null;
	for (i = 0; i < cols.length; i++) {
		if (cols[i].flex)
			flex = cols[i];
		else
			fixed += cols[i].width + 1;
	}
	if (flex)
		flex.width = Math.max(8, width - fixed - 2);

	function draw_header()
	{
		console.gotoxy(1, 1);
		console.print(C("headerTextColor") + title + " ");
		console.print(C("headerNameColor") + subtitle);
		console.cleartoeol();
		console.gotoxy(1, 2);
		console.print("\x01n" + C("colHeaderColor"));
		var s = " ";
		for (var c = 0; c < cols.length; c++)
			s += (cols[c].align == "right" ? rjust(cols[c].head, cols[c].width) : fit(cols[c].head, cols[c].width)) + " ";
		console.print(s.substr(0, width));
		console.cleartoeol();
	}

	function draw_help()
	{
		console.gotoxy(1, H);
		var bkg = "\x01" + T("helpLineBkgColor").charAt(0);
		var s = "\x01n" + bkg;
		var hl = typeof help == "function" ? help() : help;
		for (var h = 0; h < hl.length; h++)
			s += C("helpLineHotkeyColor") + bkg + hl[h][0] + C("helpLineParenColor") + bkg + ")" +
				C("helpLineGeneralColor") + bkg + hl[h][1] + " ";
		console.print(s);
		console.cleartoeol();
		console.print("\x01n");
	}

	function draw_row(idx, selected)
	{
		var y = listTop + (idx - top);
		console.gotoxy(1, y);
		if (idx >= items.length) {
			console.print("\x01n");
			console.cleartoeol();
			return;
		}
		var it = items[idx];
		var s = "\x01n" + (selected ? "\x01" + T("listHighlightBkgColor").charAt(0) : "") + " ";
		for (var c = 0; c < cols.length; c++) {
			var col = cols[c];
			var name = it.isdir && col.dircolor ? col.dircolor : col.color;
			var hiname = it.isdir && col.dircolor ? col.dirhicolor : col.hicolor;
			var text = col.get(it);
			text = col.align == "right" ? rjust(text, col.width) : fit(text, col.width);
			s += (selected ? CH(hiname) : C(name)) + text + " ";
		}
		/* pad the row to the list width (the last column belongs to the scrollbar) */
		var used = 1;
		for (c = 0; c < cols.length; c++)
			used += cols[c].width + 1;
		for (; used < width; used++)
			s += " ";
		console.print(s + "\x01n");
	}

	function draw_scrollbar()
	{
		if (items.length <= listRows)
			return;
		var thumb = Math.max(1, Math.round(listRows * listRows / items.length));
		var span = listRows - thumb;
		var pos = items.length > listRows ? Math.round(top * span / (items.length - listRows)) : 0;
		for (var r = 0; r < listRows; r++) {
			console.gotoxy(sbX, listTop + r);
			if (r >= pos && r < pos + thumb)
				console.print(C("scrollbarScrollBlockColor") + unescape_text(T("scrollbarScrollBlockChar")));
			else
				console.print(C("scrollbarBGColor") + unescape_text(T("scrollbarBGChar")));
		}
		console.print("\x01n");
	}

	function draw_list()
	{
		for (var r = 0; r < listRows; r++)
			draw_row(top + r, top + r == sel);
		draw_scrollbar();
	}

	function move_to(n)
	{
		n = Math.max(0, Math.min(items.length - 1, n));
		if (n == sel)
			return;
		var old = sel;
		sel = n;
		var oldtop = top;
		if (sel < top)
			top = sel;
		else if (sel >= top + listRows)
			top = sel - listRows + 1;
		if (top != oldtop)
			draw_list();
		else {
			draw_row(old, false);
			draw_row(sel, true);
		}
	}

	console.clear();
	draw_header();
	draw_help();
	if (!items.length) {
		console.gotoxy(2, listTop);
		console.print(unescape_text(T("emptyLibraryText")) + "\x01n");
	}
	else
		draw_list();

	while (bbs.online) {
		var key = console.getkey(K_NONE);
		if (key === "" || key === undefined)
			continue;
		switch (key) {
			case KEY_UP:
				move_to(sel - 1);
				continue;
			case KEY_DOWN:
				move_to(sel + 1);
				continue;
			case KEY_PAGEUP:
				move_to(sel - listRows);
				continue;
			case KEY_PAGEDN:
				move_to(sel + listRows);
				continue;
			case KEY_HOME:
				move_to(0);
				continue;
			case KEY_END:
				move_to(items.length - 1);
				continue;
			case "\r":
			case "\n":
				if (items.length)
					return { item: items[sel], index: sel };
				continue;
			case KEY_ESC:
			case "q":
			case "Q":
				return null;
		}
		if (onkey) {
			var handled = onkey(key, items[sel]);
			if (handled === "exit")
				return { item: items[sel], index: sel, action: key };
			if (handled) {
				draw_help();
				continue;
			}
		}
		/* digits jump to the numbered item; letters jump to the next item starting with that letter */
		if (items.length && /^[0-9]$/.test(key) && items[0].num !== undefined) {
			for (i = 0; i < items.length; i++) {
				if (items[i].num == parseInt(key, 10)) {
					move_to(i);
					break;
				}
			}
		}
		else if (items.length && /^[a-zA-Z]$/.test(key)) {
			var lk = key.toLowerCase();
			for (i = 1; i <= items.length; i++) {
				var j = (sel + i) % items.length;
				if (items[j].name.toLowerCase().charAt(0) == lk) {
					move_to(j);
					break;
				}
			}
		}
	}
	return null;
}

/* ---------- Screens ---------- */

/* S and P: cycle the text/ANSI output speed, toggle screen pausing.  Returns true if changed. */
function option_key(key)
{
	switch (key) {
		case "s":
		case "S":
			opt.speed = (opt.speed + 1) % SPEEDS.length;
			return true;
		case "p":
		case "P":
			opt.pause = !opt.pause;
			return true;
	}
	return false;
}

function option_help()
{
	return [["S", "Speed:" + (opt.speed ? SPEEDS[opt.speed] : "Full")], ["P", "Pause:" + (opt.pause ? "On" : "Off")]];
}

/* ---------- Text / ANSI viewing ---------- */

var Sauce, Graphic, Ansi, CTermLib;

function load_text_libs()
{
	if (Sauce)
		return;
	Sauce = load({}, "sauce_lib.js");
	Graphic = load({}, "graphic.js");
	Ansi = load({}, "ansiterm_lib.js");
	CTermLib = load({}, "cterm_lib.js");
	syncterm = CTermLib.query_da() !== false;
}

/* True if the user pressed Ctrl-C, Esc or Q (or Synchronet saw an abort) */
function user_interrupted()
{
	var k;
	while ((k = console.inkey(K_NONE, 0)) !== "" && k !== undefined) {
		if (k == "\x03" || k == KEY_ESC || k == "q" || k == "Q")
			return true;
	}
	if (bbs.sys_status & SS_ABORT) {
		bbs.sys_status &= ~SS_ABORT;
		return true;
	}
	return false;
}

/*
 * Send ANSI/ASCII art.  If opt.pause is set it pauses after every screenful; if cps > 0 the output is
 * paced to that many characters per second (the speed setting).  Ctrl-C, Esc or Q stops it.
 *
 * Synchronet's own pause can't be used: art that moves the cursor up (ESC[A) after each line cancels
 * the line count, so it never pauses.  The lines are counted here instead.  The speed is done here
 * as well, rather than by SyncTERM's speed emulation, because output already sent to the terminal
 * can't be interrupted.
 * Returns false if the user stopped the output.
 */
function send_art(contents, cps)
{
	var pagerows = Math.max(2, console.screen_rows - 1);
	var pagelines = 0;
	var sgr = [];		/* the colour/attribute codes in effect, so they can be restored after a prompt */
	var sent = 0;
	var t0 = system.timer;

	function track(text)
	{
		var re = /\x1b\[([0-9;]*)m/g, m;
		while ((m = re.exec(text)) !== null) {
			var codes = m[1].length ? m[1].split(";") : ["0"];
			for (var i = 0; i < codes.length; i++) {
				if (parseInt(codes[i] || "0", 10) === 0)
					sgr = [];
				else
					sgr.push(codes[i]);
			}
		}
	}

	function more()
	{
		console.write("\x1b[s");
		console.gotoxy(1, console.screen_rows);
		console.print("\x01n\x01h\x01w\x014 -- More --  any key continues, Q stops \x01n");
		console.write("\x1b[K");
		var k = console.getkey(K_NONE);
		bbs.sys_status &= ~SS_ABORT;
		console.gotoxy(1, console.screen_rows);
		console.write("\x1b[0m\x1b[K\x1b[u\x1b[0" + (sgr.length ? ";" + sgr.join(";") : "") + "m");
		t0 = system.timer - sent / cps;		/* the time spent at the prompt doesn't count */
		return !(k == "q" || k == "Q" || k == KEY_ESC || k == "\x03");
	}

	var lines = contents.split("\n");
	for (var i = 0; i < lines.length && bbs.online; i++) {
		var text = lines[i];
		var last = i == lines.length - 1;
		if (!last)
			text += "\n";
		if (!last && text.length > 1 && text.charAt(text.length - 2) != "\r")
			text = text.substr(0, text.length - 1) + "\r\n";
		if (cps > 0) {
			for (var pos = 0; pos < text.length && bbs.online;) {
				var allowed = Math.floor((system.timer - t0) * cps) - sent;
				if (allowed < 1) {
					if (user_interrupted())
						return false;
					mswait(2);
					continue;
				}
				var n = Math.min(allowed, text.length - pos);
				console.print(text.substr(pos, n), P_NOATCODES | P_NOPAUSE);
				pos += n;
				sent += n;
				if (user_interrupted())
					return false;
			}
		}
		else {
			console.print(text, P_NOATCODES | P_NOPAUSE);
			if (user_interrupted())
				return false;
		}
		track(text);
		if (!last && opt.pause && ++pagelines >= pagerows) {
			pagelines = 0;
			if (!more())
				return false;
		}
	}
	return true;
}

/* Characters per second for the current speed setting (0 = full speed) */
function art_cps()
{
	return opt.speed ? SPEEDS[opt.speed] / 10 : 0;
}

function print_art(file)
{
	if (!opt.pause && !opt.speed)
		return console.printfile(file, P_NOPAUSE | P_CPM_EOF);
	var f = new File(file);
	if (!f.open("rb"))
		throw "Failed to open " + file_getname(file);
	var contents = f.read().split("\x1a")[0];
	f.close();
	return send_art(contents, art_cps());
}

function draw_text(file, cols, rows)
{
	if (opt.speed || !cols || !rows || (rows > console.screen_rows && cols <= console.screen_columns))
		return print_art(file);
	/* the whole image fits on the screen: use Graphic */
	var graphic = new Graphic(cols, rows);
	try {
		if (graphic.load(file))
			graphic.draw();
		else
			return print_art(file);
	}
	catch (e) {
		return print_art(file);
	}
}

/* ---------- Scrollable text reader ---------- */

var READER_MAX_BYTES = 2 * 1048576;

/* Strip a UTF-8 byte-order mark and, if the text is valid UTF-8 (not CP437 art), convert it to
 * CP437; Synchronet translates CP437 for UTF-8 terminals on output. */
function normalize_text(text)
{
	text = text.replace(/^\xef\xbb\xbf/, "");
	var seq = /[\xc2-\xdf][\x80-\xbf]|[\xe0-\xef][\x80-\xbf][\x80-\xbf]|[\xf0-\xf4][\x80-\xbf][\x80-\xbf][\x80-\xbf]/g;
	if (seq.test(text) && !/[\x80-\xff]/.test(text.replace(seq, ""))) {
		load("utf8_cp437.js");
		text = utf8_cp437(text);
	}
	return text;
}

/* Split text into display lines of at most width characters */
function wrap_lines(text, width)
{
	var raw = text.split(/\r\n|\r|\n/);
	var out = [];
	for (var i = 0; i < raw.length; i++) {
		var line = raw[i].replace(/\t/g, "        ").replace(/[\x00-\x1f]/g, " ").replace(/\s+$/, "");
		while (line.length > width) {
			var brk = line.lastIndexOf(" ", width);
			if (brk < width / 2)
				brk = width;
			out.push(line.substr(0, brk).replace(/\s+$/, ""));
			line = line.substr(brk).replace(/^ /, "");
		}
		out.push(line);
	}
	return out;
}

function view_reader(file, text)
{
	var W = console.screen_columns;
	var H = console.screen_rows;
	var rows = H - 2;
	var lines = wrap_lines(normalize_text(text), W - 1);
	var maxtop = Math.max(0, lines.length - rows);
	var top = 0;
	var name = file_getname(file);

	function draw()
	{
		var pct = maxtop ? Math.round(100 * top / maxtop) : 100;
		var pos = " Line " + Math.min(lines.length, top + 1) + "-" + Math.min(lines.length, top + rows) +
			"/" + lines.length + "  " + pct + "% ";
		console.gotoxy(1, 1);
		console.print("\x01n" + C("headerTextColor") + " " + fit(name, Math.max(1, W - 1 - pos.length - 1)) +
			rjust(pos, pos.length));
		console.cleartoeol();
		for (var r = 0; r < rows; r++) {
			console.gotoxy(1, 2 + r);
			console.print("\x01n" + C("textBodyColor"));
			if (top + r < lines.length)
				console.write(lines[top + r]);
			console.cleartoeol();
		}
		if (maxtop) {
			var thumb = Math.max(1, Math.round(rows * rows / lines.length));
			var tpos = Math.round(top * (rows - thumb) / maxtop);
			for (r = 0; r < rows; r++) {
				console.gotoxy(W, 2 + r);
				if (r >= tpos && r < tpos + thumb)
					console.print(C("scrollbarScrollBlockColor") + unescape_text(T("scrollbarScrollBlockChar")));
				else
					console.print(C("scrollbarBGColor") + unescape_text(T("scrollbarBGChar")));
			}
		}
		console.gotoxy(1, H);
		var bkg = "\x01" + T("helpLineBkgColor").charAt(0);
		var help = [["Up/Dn", "Scroll"], ["PgUp/PgDn", "Page"], ["Home/End", "Top/Bottom"], ["N/P", "Next/Prev"], ["Q", "Back"]];
		var hs = "\x01n" + bkg;
		for (var h = 0; h < help.length; h++)
			hs += C("helpLineHotkeyColor") + bkg + help[h][0] + C("helpLineParenColor") + bkg + ")" +
				C("helpLineGeneralColor") + bkg + help[h][1] + " ";
		console.print(hs);
		console.cleartoeol();
		console.print("\x01n");
	}

	console.clear();
	draw();
	while (bbs.online) {
		var key = console.getkey(K_NONE);
		var old = top;
		switch (key) {
			case KEY_UP:
				top--;
				break;
			case KEY_DOWN:
			case "\r":
				top++;
				break;
			case KEY_PAGEUP:
			case "b":
			case "B":
				top -= rows - 1;
				break;
			case KEY_PAGEDN:
			case " ":
				top += rows - 1;
				break;
			case KEY_HOME:
				top = 0;
				break;
			case KEY_END:
				top = maxtop;
				break;
			case "n":
			case "N":
				return "next";
			case "p":
			case "P":
				return "prev";
			case KEY_ESC:
			case "q":
			case "Q":
				return "";
		}
		top = Math.max(0, Math.min(maxtop, top));
		if (top != old)
			draw();
	}
}

/* Returns "prev" or "next" if the user pressed P or N, otherwise "" */
function view_text(file)
{
	load_text_libs();
	/* Plain text (no ANSI sequences) goes into the scrollable reader */
	var ext = file_ext(file_getname(file));
	if (ext != "ans" && ext != "ansi" && file_size(file) <= READER_MAX_BYTES) {
		var rf = new File(file);
		if (rf.open("rb")) {
			var text = rf.read().split("\x1a")[0];
			rf.close();
			if (!/\x1b/.test(text)) {
				var nav = view_reader(file, text);
				console.clear(BG_BLACK | LIGHTGRAY);
				return nav;
			}
		}
	}
	var sauce = Sauce.read(file) || {};
	var saved_status = bbs.sys_status;
	if (opt.pause) {
		bbs.sys_status |= SS_PAUSEON;
		bbs.sys_status &= ~SS_PAUSEOFF;
	}
	else {
		bbs.sys_status &= ~SS_PAUSEON;
		bbs.sys_status |= SS_PAUSEOFF;
	}
	console.clear(BG_BLACK | LIGHTGRAY);
	var nav = "";
	function wait_key()
	{
		var k = console.getkey();
		nav = /^[nN]$/.test(k) ? "next" : /^[pP]$/.test(k) ? "prev" : "";
	}
	try {
		/* (the speed is paced by send_art(), not SyncTERM's own emulation, so that Ctrl-C can stop it) */
		if (syncterm) {
			Ansi.send("ext_mode", "clear", "cursor");
			if (sauce.ice_color)
				Ansi.send("ext_mode", "set", "bg_bright_intensity");
			mswait(500);
		}
		var finished = draw_text(file, sauce.cols, sauce.rows);
		if (finished !== false)		/* if the user interrupted it, go straight back to the list */
			wait_key();
		if (syncterm) {
			Ansi.send("ext_mode", "clear", "bg_bright_intensity");
			Ansi.send("ext_mode", "set", "cursor");
		}
	}
	finally {
		bbs.sys_status = saved_status;
		console.clear(BG_BLACK | LIGHTGRAY);
	}
	return nav;
}

/* ---------- ZIP extraction ---------- */

function zip_dest(zipfile)
{
	return zipfile.replace(/\.[^.\/\\]*$/, "");
}

/* Delete a directory and everything in it (used to clean up after a failed extraction) */
function remove_tree(dir)
{
	var paths = directory(backslash(dir) + "*");
	for (var i = 0; i < paths.length; i++) {
		var p = paths[i].replace(/[\\\/]+$/, "");
		if (file_isdir(p))
			remove_tree(p);
		else
			file_remove(p);
	}
	rmdir(dir);
}

/* Extract a ZIP next to itself into a directory of the same name (once). Returns an error string or null. */
function extract_zip(entry)
{
	var dest = zip_dest(entry.path);
	if (file_isdir(dest))
		return null;
	if (file_exists(dest))
		return "Cannot extract: " + file_getname(dest) + " already exists";
	try {
		var a = new Archive(entry.path);
		var list = a.list();
		var total = 0;
		var count = 0;
		for (var i = 0; i < list.length; i++) {
			var n = String(list[i].name);
			if (/^([a-zA-Z]:)?[\\\/]/.test(n) || /(^|[\\\/])\.\.([\\\/]|$)/.test(n))
				return "Archive contains unsafe file names";
			if (list[i].type == "file") {
				count++;
				total += list[i].size;
			}
		}
		if (count > opt.zip_max_files || total > opt.zip_max_bytes)
			return "Archive is too large to extract";
		if (!mkdir(dest))
			return "Unable to create " + file_getname(dest);
		try {
			a.extract(dest, true, true, opt.zip_max_files);
			return null;
		}
		catch (e) {
			/* e.g. old "imploded" ZIPs that libarchive can't read: start again with unzip, if there is one */
			log(LOG_INFO, "sixelviewlibrary: " + entry.path + ": " + e + " (trying unzip)");
		}
		remove_tree(dest);
		if (/["$`\\!]/.test(entry.path + dest))
			return "Unable to extract " + entry.name;
		var unzip = file_exists(system.exec_dir + "unzip") ? system.exec_dir + "unzip" : "unzip";
		if (!mkdir(dest))
			return "Unable to create " + file_getname(dest);
		var rc = system.exec(unzip + ' -o -qq -d "' + dest + '" "' + entry.path + '"');
		if ((rc !== 0 && rc !== 1) || directory(backslash(dest) + "*").length < 1) {
			log(LOG_WARNING, "sixelviewlibrary: unzip of " + entry.path + " failed, code " + rc);
			remove_tree(dest);
			return "Unable to extract " + entry.name;
		}
	}
	catch (e) {
		log(LOG_WARNING, "sixelviewlibrary: extracting " + entry.path + ": " + e);
		remove_tree(dest);
		return "Unable to extract " + entry.name;
	}
	return null;
}

function view_file(entry)
{
	var doc;
	try {
		if (TEXT_TYPES.indexOf(file_ext(entry.name)) >= 0) {
			return view_text(entry.path);
		}
		var f = new File(entry.path);
		var head = "";
		if (f.exists && f.open("rb")) {
			head = f.read(1024);
			f.close();
		}
		if (head.indexOf("%PDF-") >= 0)
			doc = load_pdf(entry.path);
		else
			doc = new ImageDoc(load_image_file(entry.path));
		return viewer(doc, 0, true) || "";
	}
	catch (e) {
		console.clear();
		pause_error(unescape_text(T("errorText")).replace("%s", String(e).replace(/\x01/g, "")));
	}
	return "";
}

/* Fetch (for online libraries) and view a file.  Returns "prev"/"next" from the text viewer, else "" */
function open_entry(lib, hook, subpath, e, image)
{
	if (lib.module) {
		console.gotoxy(1, console.screen_rows);
		console.print(unescape_text(T("loadingText")) + "\x01n");
		console.cleartoeol();
		var path = hook && hook.fetch ? hook.fetch(subpath, e, image ? "image" : undefined) : null;
		if (path && image && file_size(path) > opt.image_max_bytes) {
			console.clear();
			pause_error(unescape_text(T("errorText")).replace("%s", "image is too large"));
			return "";
		}
		if (!path) {
			console.clear();
			pause_error(unescape_text(T("errorText")).replace("%s", "download failed"));
			return "";
		}
		e = { name: image ? e.name + ".png" : e.name, path: path, isdir: false };
	}
	return view_file(e);
}

/* Index of the next/previous viewable file from index i, or -1 */
function neighbour(entries, i, dir)
{
	for (i += dir; i >= 0 && i < entries.length; i += dir) {
		if (!entries[i].isdir && !entries[i].iszip)
			return i;
	}
	return -1;
}

function browse_library(lib)
{
	var subpath = [];
	var sel = 0;		/* item to put the cursor on */
	var parents = [];	/* selected index in each parent directory */
	while (bbs.online) {
		var entries;
		if (lib.module) {
			console.clear();
			console.print(unescape_text(T("loadingText")) + "\x01n");
			entries = list_hook_entries(lib, subpath);
			if (!entries) {
				console.clear();
				pause_error(unescape_text(T("hookErrorText")));
				if (!subpath.length)
					return;
				subpath.pop();
				sel = parents.pop() || 0;
				continue;
			}
		}
		else
			entries = list_entries(lib, subpath);
		var cols = [
			{ head: "Name", flex: true, color: "listNameColor", hicolor: "listNameHighlightColor",
				dircolor: "listDirColor", dirhicolor: "listDirHighlightColor",
				get: function (e) { return e.label ? e.label : e.isdir && !e.up ? e.name + "/" : e.name; } },
			{ head: "Type", width: 4, color: "listTypeColor", hicolor: "listTypeHighlightColor",
				get: function (e) { return e.isdir ? (e.up ? "" : "DIR") : e.type; } },
			{ head: "Size", width: 6, align: "right", color: "listSizeColor", hicolor: "listSizeHighlightColor",
				get: function (e) { return e.isdir || !e.size ? "" : format_size(e.size); } },
			{ head: "Date", width: 10, color: "listDateColor", hicolor: "listDateHighlightColor",
				get: function (e) { return e.up || !e.date ? "" : format_date(e.date); } }
		];
		var where = lib.name + (subpath.length ? "/" + subpath.join("/") : "");
		var hook = lib.module ? open_hook(lib) : null;
		var has_images = hook && hook.images;
		var help = function () {
			return [["Enter", "View"]].concat(has_images ? [["I", "Image"]] : [], [["Up/Dn", "Select"]],
				option_help(), [["Q", "Back"]]);
		};
		var onkey = function (key, item) {
			if (has_images && (key == "i" || key == "I"))
				return item && !item.isdir ? "exit" : false;
			return option_key(key);
		};
		var res = lightbar(unescape_text(T("titleText")).replace(/\x01./g, ""), where, cols, entries, help, onkey, sel);
		if (!res)
			return;
		var e = res.item;
		sel = res.index;
		if (e.up) {
			subpath.pop();
			sel = parents.pop() || 0;
		}
		else if (e.isdir) {
			subpath.push(e.name);
			parents.push(sel);
			sel = 0;
		}
		else if (e.iszip) {
			var err = extract_zip(e);
			if (err) {
				console.clear();
				pause_error(unescape_text(T("errorText")).replace("%s", err));
			}
			else {
				subpath.push(file_getname(zip_dest(e.path)));
				parents.push(sel);
				sel = 0;
			}
		}
		else {
			var image = res.action == "i" || res.action == "I";
			var nav = open_entry(lib, hook, subpath, e, image);
			/* N and P in the viewers move on to the next/previous file */
			while (nav == "prev" || nav == "next") {
				var n = neighbour(entries, sel, nav == "next" ? 1 : -1);
				if (n < 0)
					break;
				sel = n;
				nav = open_entry(lib, hook, subpath, entries[n], false);
			}
		}
	}
}

function main()
{
	var settings = read_settings();
	if (!settings) {
		console.print("\x01n\x01h\x01rSixelView Library is not configured (settings.ini not found).\x01n\r\n");
		console.pause();
		return;
	}
	read_theme(settings.theme);

	var libs = [];
	var have_hook = false;
	for (var i = 0; i < settings.libraries.length; i++) {
		if (library_allowed(settings.libraries[i])) {
			libs.push(settings.libraries[i]);
			if (settings.libraries[i].module)
				have_hook = true;
		}
	}
	if (have_hook)
		clean_cache();
	if (!libs.length) {
		console.print(unescape_text(T("noLibrariesText")) + "\x01n\r\n");
		console.pause();
		return;
	}

	/* Only one library available: go straight into it */
	if (libs.length == 1) {
		browse_library(libs[0]);
		return;
	}

	var items = [];
	for (i = 0; i < libs.length; i++) {
		items.push({ lib: libs[i], num: i + 1, name: libs[i].name, desc: libs[i].desc,
			files: libs[i].subdirs || libs[i].module ? "" : String(count_files(libs[i])) });
	}
	var cols = [
		{ head: "#", width: 3, align: "right", color: "listNumColor", hicolor: "listNumHighlightColor",
			get: function (it) { return it.num; } },
		{ head: "Library", width: 24, color: "listNameColor", hicolor: "listNameHighlightColor",
			get: function (it) { return it.name; } },
		{ head: "Description", flex: true, color: "listDescColor", hicolor: "listDescHighlightColor",
			get: function (it) { return it.desc; } },
		{ head: "Files", width: 5, align: "right", color: "listSizeColor", hicolor: "listSizeHighlightColor",
			get: function (it) { return it.files; } }
	];
	var help = function () {
		return [["Enter", "Open"], ["Up/Dn", "Select"]].concat(option_help(), [["Q", "Quit"]]);
	};
	var sel = 0;
	while (bbs.online) {
		var res = lightbar(unescape_text(T("titleText")).replace(/\x01./g, ""), "", cols, items, help, option_key, sel);
		if (!res)
			break;
		sel = res.index;
		browse_library(res.item.lib);
	}
}

try {
	main();
	console.attributes = 7;
	console.clear();
}
catch (e) {
	log(LOG_ERR, "sixelviewlibrary error: " + e + (e && e.stack ? "\n" + e.stack : ""));
	console.print("\x01n\x01h\x01rsixelviewlibrary error: \x01w" + String(e).replace(/\x01/g, "") + "\x01n\r\n");
}
