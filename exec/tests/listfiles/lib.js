// listfiles_lib.js: options merging, header/footer menu lookup order,
// filespec widening, module-local @-codes and pagination

// Loaded by path first: jsdoor has no exec dir to resolve the library's own
// bare require("attr_option.js") against, and require() skips a file whose
// symbol is already defined on this scope object. jsdoor's VAROBJFIX option
// binds the loaded file's functions to the global object instead, so the
// symbol is put on this scope by hand.
load(js.exec_dir + "../../load/attr_option.js");
this.attr_option = attr_option;
load(js.exec_dir + "../../load/listfiles_lib.js");

var failures = [];

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

check("listfiles_options", listfiles_options({a:1, b:2}, {b:3}, null, {c:4}), {a:1, b:3, c:4});
check("listfiles_options undefined", listfiles_options({a:1}, undefined, {a:2}, undefined), {a:2});

check("listfiles_menu_candidates"
	, listfiles_menu_candidates("UPLOADS", 3, "head")
	, ["listfiles_uploads_head", "listfiles_lib3_head", "listfiles_head"]);
check("listfiles_menu_candidates tail"
	, listfiles_menu_candidates("main", 1, "tail")
	, ["listfiles_main_tail", "listfiles_lib1_tail", "listfiles_tail"]);

check("widen 12+ with ext", listfiles_widen_filespec("abcdefghijkl.zip"), "abcdefghijkl*.zip");
check("widen 12+ no ext", listfiles_widen_filespec("abcdefghijklmn"), "abcdefghijklmn*");
check("widen short", listfiles_widen_filespec("short.zip"), "short.zip");
check("widen wildcard", listfiles_widen_filespec("abcdefghijkl*.zip"), "abcdefghijkl*.zip");
check("widen empty", listfiles_widen_filespec(""), "");

check("local codes", listfiles_local_codes("[@LETTER@][@SIZE-R7@]", {letter:"C", size:"1.5M"}), "[C][   1.5M]");
check("local -L", listfiles_local_codes("@FLAGS-L3@|", {flags:"+"}), "+  |");
check("local |L", listfiles_local_codes("@TAG|L2@|", {tag:"*"}), "* |");
check("local -C", listfiles_local_codes("@LETTER-C3@", {letter:"A"}), " A ");
check("local truncates", listfiles_local_codes("@SIZE-R3@", {size:"12345"}), "123");
check("local missing", listfiles_local_codes("@MISSING@", {missing:"-"}), "-");
check("local undefined value", listfiles_local_codes("[@TAG@]", {}), "[]");
check("unknown left alone", listfiles_local_codes("@FILE_NAME-L12@ @LETTER@", {letter:"B"}), "@FILE_NAME-L12@ B");

check("local name/desc/attrs"
	, listfiles_local_codes("@NAME_ATTR@@NAME@@LETTER_ATTR@@LETTER@@SIZE_ATTR@@SIZE-R6@@DESC_ATTR@@DESC@"
		, {name:"a.zip", name_attr:"<n>", letter:"A", letter_attr:"<l>", size:"10K", size_attr:"<s>"
		, desc:"An archive", desc_attr:"<d>"})
	, "<n>a.zip<l>A<s>   10K<d>An archive");

function fake_expander(str) { return str.replace(/@FILE_DATE@/g, "11/17/26"); }
check("expand", listfiles_expand("@NAME-L6@|@FILE_DATE@|@DESC@", {name:"a.zip", desc:"@FILE_DATE@ x"}, fake_expander)
	, "a.zip |11/17/26|@FILE_DATE@ x");
var expander_calls = 0;
listfiles_expand("@NAME@@LETTER@", {name:"x"}, function (s) { expander_calls++; return s; });
check("expander skipped without @", expander_calls, 0);

check("fname short", listfiles_format_filename("readme.txt", 12), "readme  .txt");
check("fname long", listfiles_format_filename("averyverylongname.zip", 12), "averyver.zip");
check("fname no ext", listfiles_format_filename("noextensionlongname", 12), "noextensionl");
check("fname no ext short", listfiles_format_filename("noext", 12), "noext       ");
check("fname long ext", listfiles_format_filename("x.verylongextension", 12), "x.verylongex");
check("fname exact", listfiles_format_filename("abcdefgh.zip", 12), "abcdefgh.zip");

check("size 0", listfiles_size_str(0), "0");
check("size 1023", listfiles_size_str(1023), "1023");
check("size 1536", listfiles_size_str(1536), "1.5K");
check("size 999999", listfiles_size_str(999999), "976.6K");
check("size 1048575", listfiles_size_str(1048575), "1.0M");
check("size 1572864", listfiles_size_str(1572864), "1.5M");
check("size 10G", listfiles_size_str(10737418240), "10.0G");

check("attr 0x07", listfiles_attr_str(0x07), "\x01n\x01w");
check("attr 0x0F", listfiles_attr_str(0x0F), "\x01n\x01w\x01h");
check("attr 0x1E", listfiles_attr_str(0x1E), "\x01n\x01y\x01h\x014");
check("attr 0x8C", listfiles_attr_str(0x8C), "\x01n\x01r\x01h\x01i");

check("window count", listfiles_window_count([1, 1, 1, 1, 1], 0, 2), 2);
check("window count at end", listfiles_window_count([1, 1, 1, 1, 1], 4, 2), 1);
check("window count tall item alone", listfiles_window_count([1, 5, 1], 1, 3), 1);
check("window count mixed", listfiles_window_count([2, 1, 3, 1], 0, 4), 2);
check("window count empty", listfiles_window_count([], 0, 10), 0);
check("window top", listfiles_window_top([1, 1, 1, 1, 1], 4, 2), 3);
check("window top after tall item", listfiles_window_top([1, 5, 1], 2, 3), 2);
check("window top mixed", listfiles_window_top([2, 1, 3, 1], 3, 4), 2);
check("window top everything fits", listfiles_window_top([1, 1], 1, 5), 0);
check("window top tall bottom", listfiles_window_top([1, 9], 1, 3), 1);

function render_names(n) { return "L " + n[0] + " D " + n[1] + " (5 Files): "; }
function plain_len(str) { return str.length; }
check("fit: already fits", listfiles_shorten_to_fit(["Main", "Uploads"], render_names, plain_len, 40)
	, "L Main D Uploads (5 Files): ");
check("fit: longer name shortened first"
	, listfiles_shorten_to_fit(["Lib", "Directory-name"], render_names, plain_len, 30)
	, "L Lib D Directory- (5 Files): ");
check("fit: both shortened, end kept"
	, listfiles_shorten_to_fit(["Library", "Directory"], render_names, plain_len, 24)
	, "L Libr D Dir (5 Files): ");
check("fit: can't fit, names down to one character"
	, listfiles_shorten_to_fit(["Library", "Directory"], render_names, plain_len, 10)
	, "L L D D (5 Files): ");

check("extdesc lines: CR/LF", listfiles_extdesc_lines("one\r\ntwo\r\n"), ["one", "two"]);
check("extdesc lines: leading blank line and normal attr dropped"
	, listfiles_extdesc_lines("\r\n\x01nfirst\r\nsecond"), ["first", "second"]);
check("extdesc lines: conditional newline breaks after text"
	, listfiles_extdesc_lines("a\x01/b\x01/c"), ["a", "b", "c"]);
check("extdesc lines: conditional newline at a line's start does nothing"
	, listfiles_extdesc_lines("a\r\n\x01/b"), ["a", "b"]);
check("extdesc lines: conditional newline after only color codes does nothing"
	, listfiles_extdesc_lines("\x01h\x01/x\x01/"), ["\x01hx"]);
check("extdesc lines: a line of just a conditional newline is still a line"
	, listfiles_extdesc_lines("a\r\n\x01/\r\nb"), ["a", "", "b"]);
check("extdesc lines: color codes carried to the next line"
	, listfiles_extdesc_lines("\x01+\x01ra\x01/\x01-b"), ["\x01+\x01ra", "\x01-b"]);

check("style default", listfiles_style("", true, true), "classic");
check("style lightbar", listfiles_style("lightbar", true, true), "lightbar");
check("style classic", listfiles_style("classic", true, true), "classic");
check("style without batch flagging", listfiles_style("lightbar", false, true), "plain");
check("style on a dumb terminal", listfiles_style("lightbar", true, false), "letters");
check("style unknown", listfiles_style("bogus", true, true), "classic");
check("style undefined", listfiles_style(undefined, true, true), "classic");

check("rows: no pane", listfiles_lightbar_rows(24, true, false)
	, {list_top:2, list_rows:21, pane_top:0, pane_rows:0});
check("rows: pane", listfiles_lightbar_rows(24, true, true)
	, {list_top:2, list_rows:11, pane_top:14, pane_rows:9});
check("rows: too short for the pane", listfiles_lightbar_rows(10, true, true)
	, {list_top:2, list_rows:7, pane_top:0, pane_rows:0});
check("rows: no headings", listfiles_lightbar_rows(24, false, true).list_top, 1);
check("rows: 19-row screen, no pane", listfiles_lightbar_rows(19, true, true).pane_rows, 0);
check("rows: 20-row screen, a pane", listfiles_lightbar_rows(20, true, true).pane_rows > 0, true);
check("rows: no headings, all rows used", listfiles_lightbar_rows(24, false, false).list_rows, 22);

check("mnemonics: fits", listfiles_mnemonics_cut("~Download ~Quit", 79), "~Download ~Quit");
check("mnemonics: cut, ~ not counted", listfiles_mnemonics_cut("~Download ~Quit", 9), "~Download ");
check("mnemonics: no dangling ~", listfiles_mnemonics_cut("ab~Cd", 2), "ab");
check("mnemonics: Ctrl-A codes not counted", listfiles_mnemonics_cut("\x01hab", 2), "\x01hab");

var titles = { name: "Name", size: "Size", desc: "Description" };
check("headings: default line", listfiles_headings(listfiles_defaults().line_fmt_lightbar, titles, 12)
	, "    Name        Size Description");
check("headings: wider name column, the name centered", listfiles_headings("@NAME@ @DESC@", titles, 20)
	, "        Name         Description");
check("headings: other codes are blank, their width kept"
	, listfiles_headings("@NAME@@FILE_DATE-R8@|@FILE_UPLOADER@|@TAG@|", titles, 4), "Name        || |");
check("headings: colors and attribute codes dropped"
	, listfiles_headings("\x01h@NAME_ATTR@@NAME@\x01n@SIZE_ATTR@@SIZE-R6@", titles, 4), "Name  Size");
check("headings: a modifier without a width is the code's width"
	, listfiles_headings("@FILE_DATE-L@|", titles, 4), "             |");
check("headings: text kept", listfiles_headings("[@NAME@]", titles, 4), "[Name]");

check("pane scroll: down", listfiles_pane_scroll(0, "down", 20, 5), 1);
check("pane scroll: up at the top", listfiles_pane_scroll(0, "up", 20, 5), 0);
check("pane scroll: page down", listfiles_pane_scroll(0, "pgdn", 20, 5), 5);
check("pane scroll: page down stops at the last page", listfiles_pane_scroll(13, "pgdn", 20, 5), 15);
check("pane scroll: page up", listfiles_pane_scroll(7, "pgup", 20, 5), 2);
check("pane scroll: home", listfiles_pane_scroll(7, "home", 20, 5), 0);
check("pane scroll: end", listfiles_pane_scroll(0, "end", 20, 5), 15);
check("pane scroll: everything fits", listfiles_pane_scroll(0, "down", 3, 5), 0);
check("pane scroll: unknown", listfiles_pane_scroll(4, "x", 20, 5), 4);

var queue = listfiles_queued([{ name: "A.ZIP", dir: "MAIN" }, { name: "b.txt", dir: "other" }
	, { name: "c.txt" }], "main");
check("queued: this directory's", queue["a.zip"], true);
check("queued: another directory's", queue["b.txt"], undefined);
check("queued: no directory", queue["c.txt"], undefined);
check("queued: none", listfiles_queued(null, "main"), {});

check("strip colors: Ctrl-A codes removed", listfiles_strip_colors("\x01h\x01wab\x01nc"), "abc");
check("strip colors: CP437 control-range characters kept", listfiles_strip_colors("\x01ha\x19\xfbb"), "a\x19\xfbb");

check("key tips: no bar color, unchanged", listfiles_key_tips("~Download, ~Quit", "", "<k>"), "~Download, ~Quit");
check("key tips: bar and key colors", listfiles_key_tips("~Download, ~Quit", "<b>", "<k>")
	, "<b>\x01+<k>~D\x01-ownload, \x01+<k>~Q\x01-uit");
check("key tips: no key color", listfiles_key_tips("~Dl", "<b>", ""), "<b>\x01+~D\x01-l");

var d = listfiles_defaults();
check("key tips bar default (off)", d.lightbar_keys_attr, "");
check("queued_marker default (down arrow)", d.queued_marker, "\x19");
check("preview_extdesc_attr default (bright white)", d.preview_extdesc_attr, "WH");
check("every preview info line sets its own color"
	, d.preview_info_fmt.split("\r\n").filter(function (l) { return l !== "" && l.indexOf("\x01n") !== 0; }), []);
check("highlight_line default", d.highlight_line, true);
check("no lightbar option (renamed highlight_line)", d.lightbar, undefined);
check("selected_item_attr default (the mnemonic color)", d.selected_item_attr, "V");
check("lightbar_attr default (bright white on blue)", d.lightbar_attr, "WH4");
var attr_keys = [];
for (var k in d) {
	if (/_attr$/.test(k) && d[k] !== "" && d[k].indexOf("\x01") >= 0)
		attr_keys.push(k);
}
check("attr defaults are attribute letters", attr_keys, []);
check("headings default: generated", d.lightbar_headings, true);
check("heading titles", [d.heading_name, d.heading_size, d.heading_desc], ["Name", "Size", "Description"]);
check("default style", d.style, "classic");
check("lightbar keys fit", strip_ctrl(d.lightbar_keys.replace(/~/g, "")).length <= 79, true);
check("operator keys fit", strip_ctrl(d.lightbar_keys_operator.replace(/~/g, "")).length <= 79, true);
check("operator keys add Remove", d.lightbar_keys_operator.indexOf("~Remove") >= 0, true);
check("lightbar keys end with help", d.lightbar_keys.substr(-2), "~?");
check("key tips prefix: a low-intensity cyan block", d.lightbar_keys_prefix, "\x01n\x01c\xfe ");
check("key tips have no Ctrl-A codes (mnemonics coloring)", d.lightbar_keys.indexOf("\x01"), -1);
check("lightbar line has no letter", d.line_fmt_lightbar.indexOf("@LETTER@"), -1);
check("interactive line has no letter", d.line_fmt_interactive.indexOf("@LETTER@"), -1);
check("tagged marker is the CP437 check mark", d.tagged_marker, "\xfb");
check("interactive commands name Space", strip_ctrl(d.interactive_commands || "").indexOf("[Space] Tag") >= 0, true);
check("interactive commands name Toggle", strip_ctrl(d.interactive_commands || "").indexOf("[T]oggle Desc") >= 0, true);
check("interactive commands omit Previous", strip_ctrl(d.interactive_commands || "").indexOf("[P]revious"), -1);
check("interactive commands don't clear the screen (or a header above them)", (d.interactive_commands || "").toLowerCase().indexOf("\x01l"), -1);
check("interactive commands width", strip_ctrl(d.interactive_commands || "").replace(/\r\n$/, "").length <= 79, true);
check("more_above default", d.more_above, "\x18");
check("more_below default", d.more_below, "\x19");
check("no_more default", d.no_more, "\xfe");
check("interactive prompt starts in the normal attribute", (d.interactive_prompt || "").substr(0, 2), "\x01n");
check("position format", strip_ctrl(format(d.position_fmt || "", 3, 99)), "[file 3 of 99]");
check("interactive prompt"
	, strip_ctrl(format(d.interactive_prompt || "", "<", ">", 1, "Main", 2, "Synchronet", 99))
	, "< Listing > (1) Main (2) Synchronet (?=Menu) (99 Files): ");
check("interactive line has the tag", d.line_fmt_interactive.indexOf("@TAG@") >= 0, true);
check("interactive flag column has its own color", d.line_fmt_interactive.indexOf("@TAG_ATTR@@TAG@") >= 0, true);
check("lightbar flag column has its own color", d.line_fmt_lightbar.indexOf("@TAG_ATTR@@TAG@") >= 0, true);
check("interactive + marker has its own color", d.line_fmt_interactive.indexOf("@FLAGS_ATTR@@FLAGS@") >= 0, true);
check("lightbar + marker has its own color", d.line_fmt_lightbar.indexOf("@FLAGS_ATTR@@FLAGS@") >= 0, true);
check("letter flow + marker has its own color", d.line_fmt.indexOf("@FLAGS_ATTR@@FLAGS@") >= 0, true);
check("plain list + marker has its own color", d.line_fmt_nobatch.indexOf("@FLAGS_ATTR@@FLAGS@") >= 0, true);
check("flags_attr default (dark grey)", d.flags_attr, "KH");
check("local FLAGS_ATTR", listfiles_local_codes("@FLAGS_ATTR@@FLAGS@", {flags_attr:"<f>", flags:"+"}), "<f>+");
check("tag_attr default (bright white)", d.tag_attr, "WH");
check("local TAG_ATTR", listfiles_local_codes("@TAG_ATTR@@TAG@", {tag_attr:"<t>", tag:"x"}), "<t>x");
var keys = ["line_fmt", "line_fmt_nobatch", "line_fmt_interactive", "extdesc_marker", "missing_marker"
	, "tagged_marker", "untagged_marker", "box_top_left", "box_top_right"
	, "box_bottom_left", "box_bottom_right", "box_horizontal", "box_vertical"
	, "short_hdr_underline", "selected_item_attr", "highlight_line"];
for (var i = 0; i < keys.length; i++) {
	if (d[keys[i]] === undefined)
		failures.push("listfiles_defaults() lacks " + keys[i]);
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
