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

var d = listfiles_defaults();
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
