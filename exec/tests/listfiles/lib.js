// listfiles_lib.js: options merging, header/footer menu lookup order,
// filespec widening, module-local @-codes and pagination

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
check("rows: no headings, all rows used", listfiles_lightbar_rows(24, false, false).list_rows, 22);

check("E hidden", listfiles_pane_key("E", false, "contents"), {shown:true, view:"info"});
check("E info", listfiles_pane_key("E", true, "info"), {shown:false, view:"info"});
check("E contents", listfiles_pane_key("E", true, "contents"), {shown:true, view:"info"});
check("V hidden", listfiles_pane_key("V", false, "info"), {shown:true, view:"contents"});
check("V info", listfiles_pane_key("V", true, "info"), {shown:true, view:"contents"});
check("V contents", listfiles_pane_key("V", true, "contents"), {shown:false, view:"contents"});
check("other key", listfiles_pane_key("X", true, "info"), {shown:true, view:"info"});

check("contents: no method", listfiles_contents_allowed(undefined, {preview_contents:true}), false);
check("contents: turned off", listfiles_contents_allowed(function () {}, {preview_contents:false}), false);
check("contents: allowed", listfiles_contents_allowed(function () {}, {preview_contents:true}), true);

check("text: ANSI and control characters removed"
	, listfiles_text_lines("a\x1b[1;31mb\r\nc\x07d", 10, 79), ["ab", "cd"]);
check("text: lone ESC removed", listfiles_text_lines("a\x1bb", 10, 79), ["ab"]);
check("text: tab to a space", listfiles_text_lines("a\tb", 10, 79), ["a b"]);
check("text: Ctrl-A codes kept", listfiles_text_lines("\x01hbold", 10, 79), ["\x01hbold"]);
check("text: cut to width", listfiles_text_lines("abcdefghij", 10, 4), ["abcd"]);
check("text: width counts printable characters", listfiles_text_lines("\x01habcdef", 10, 3), ["\x01habc"]);
check("text: max lines", listfiles_text_lines("1\n2\n3\n4", 2, 79), ["1", "2"]);

var entries = [{type:"directory", name:"d/", size:0, time:0}
	, {type:"file", name:"a.txt", size:12, time:0}
	, {type:"file", name:format("%0200d", 0), size:5, time:0}];
var alines = listfiles_archive_lines(entries, 10, 79);
check("archive: directories skipped", alines.length, 2);
check("archive: name first", alines[0].substr(0, 5), "a.txt");
check("archive: long name cut to fit", alines[1].length <= 79, true);
check("archive: max lines", listfiles_archive_lines(entries, 1, 79).length, 1);

check("contents: a reader that throws"
	, listfiles_contents_lines(function () { throw new Error("bad zip"); }), {lines:[], note:"bad zip"});
check("contents: a reader with no result", listfiles_contents_lines(function () { return null; }).lines, []);
check("contents: a reader's lines", listfiles_contents_lines(function () { return ["x"]; }), {lines:["x"], note:""});

check("mnemonics: fits", listfiles_mnemonics_cut("~Download ~Quit", 79), "~Download ~Quit");
check("mnemonics: cut, ~ not counted", listfiles_mnemonics_cut("~Download ~Quit", 9), "~Download ");
check("mnemonics: no dangling ~", listfiles_mnemonics_cut("ab~Cd", 2), "ab");
check("mnemonics: Ctrl-A codes not counted", listfiles_mnemonics_cut("\x01hab", 2), "\x01hab");

var titles = { name: "Name", size: "Size", desc: "Description" };
check("headings: default line", listfiles_headings(listfiles_defaults().line_fmt_lightbar, titles, 12)
	, "Name            Size Description");
check("headings: wider name column", listfiles_headings("@NAME@ @DESC@", titles, 20)
	, "Name                 Description");
check("headings: other codes are blank, their width kept"
	, listfiles_headings("@NAME@@FILE_DATE-R8@|@FILE_UPLOADER@|@TAG@|", titles, 4), "Name        || |");
check("headings: colors and attribute codes dropped"
	, listfiles_headings("\x01h@NAME_ATTR@@NAME@\x01n@SIZE_ATTR@@SIZE-R6@", titles, 4), "Name  Size");
check("headings: a modifier without a width is the code's width"
	, listfiles_headings("@FILE_DATE-L@|", titles, 4), "             |");
check("headings: text kept", listfiles_headings("[@NAME@]", titles, 4), "[Name]");

var d = listfiles_defaults();
check("headings default: generated", d.lightbar_headings, true);
check("heading titles", [d.heading_name, d.heading_size, d.heading_desc], ["Name", "Size", "Description"]);
check("default style", d.style, "classic");
check("lightbar keys fit", strip_ctrl(d.lightbar_keys.replace(/~/g, "")).length <= 79, true);
check("operator keys fit", strip_ctrl(d.lightbar_keys_operator.replace(/~/g, "")).length <= 79, true);
check("operator keys add Remove", d.lightbar_keys_operator.indexOf("~Remove") >= 0, true);
check("lightbar keys end with help", d.lightbar_keys.substr(-2), "~?");
check("lightbar line has no letter", d.line_fmt_lightbar.indexOf("@LETTER@"), -1);
check("preview size limit", d.preview_max_size, 52428800);
check("preview delay", d.preview_delay, 0.5);
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
check("tag_attr default (bright white)", d.tag_attr, "\x01n\x01h\x01w");
check("local TAG_ATTR", listfiles_local_codes("@TAG_ATTR@@TAG@", {tag_attr:"<t>", tag:"x"}), "<t>x");
var keys = ["line_fmt", "line_fmt_nobatch", "line_fmt_interactive", "extdesc_marker", "missing_marker"
	, "tagged_marker", "untagged_marker", "box_top_left", "box_top_right"
	, "box_bottom_left", "box_bottom_right", "box_horizontal", "box_vertical"
	, "short_hdr_underline", "selected_item_attr", "lightbar"];
for (var i = 0; i < keys.length; i++) {
	if (d[keys[i]] === undefined)
		failures.push("listfiles_defaults() lacks " + keys[i]);
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
