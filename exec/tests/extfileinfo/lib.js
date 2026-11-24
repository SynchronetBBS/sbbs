// extfileinfo_lib.js: search argument decoding, number and size formatting

load(js.exec_dir + "../../load/extfileinfo_lib.js");

var failures = [];

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

check("decode none", extfileinfo_decode_search([]), "");
check("decode words", extfileinfo_decode_search(["SOUL", "DOUBT"]), "SOUL DOUBT");
check("decode quotes", extfileinfo_decode_search(["%22NO", "FX%22", "|", "100%25"]), "\"NO FX\" | 100%");
check("decode literal %41 survives", extfileinfo_decode_search(["%2541"]), "%41");
check("commas", extfileinfo_commas(825824634), "825,824,634");
check("commas small", extfileinfo_commas(0), "0");
check("estimate under 1K", extfileinfo_byte_estimate(1000), "1.0K");
check("estimate small", extfileinfo_byte_estimate(100), "0.1K");
check("estimate empty", extfileinfo_byte_estimate(0), "0.0K");
check("file time", extfileinfo_file_time(1304653324), 1304653324);
check("file time missing (-1 as unsigned)", extfileinfo_file_time(4294967295), 0);
check("file time none", extfileinfo_file_time(0), 0);
check("estimate K", extfileinfo_byte_estimate(1024), "1.0K");
check("estimate M", extfileinfo_byte_estimate(825824634), "787.6M");
check("estimate G", extfileinfo_byte_estimate(1825361100), "1.7G");

// text.dat strings: @-codes expanded only in one with no % (as bprintf() does)
check("text without %: printed as is, @-codes expanded", extfileinfo_text("File: @FILE_NAME@", ["x"]), { str: "File: @FILE_NAME@", atcodes: true });
check("text with %: formatted, @-codes not expanded", extfileinfo_text("File: %s", ["@USER@"]), { str: "File: @USER@", atcodes: false });
check("text with two arguments", extfileinfo_text("(%u) %s", [3, "Main"]), { str: "(3) Main", atcodes: false });

// The highlight color: found_attr, else bright yellow (never a user theme
// color, which the field labels and values are in)
check("found_attr set", extfileinfo_found_attr("\x01n\x01M\x01H"), "\x01n\x01M\x01H");
check("found_attr empty: bright yellow", extfileinfo_found_attr(""), "\x01n\x01Y\x01H");

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
