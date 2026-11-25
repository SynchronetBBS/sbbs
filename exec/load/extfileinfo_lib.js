// Library for the Extended File Info loadable module (extfileinfo.js):
// decoding its search argument, and number and size formatting as the
// built-in file information display does it
// for Synchronet v3.22+

// Usage: load("extfileinfo_lib.js");

"use strict";

// The search expression from the module's arguments after the first three:
// joined with spaces, then each %XX (hex) decoded (the Terminal Server
// encodes % and " in it, as hex_encode() does)
function extfileinfo_decode_search(args)
{
	return args.join(" ").replace(/%([0-9A-Fa-f]{2})/g, function (match, hex) {
		return String.fromCharCode(parseInt(hex, 16));
	});
}

// 'n' (a whole number) with a comma between each group of three digits
function extfileinfo_commas(n)
{
	return String(Math.floor(n)).replace(/\B(?=(\d{3})+$)/g, ",");
}

// A byte count estimated in the largest 1024-based unit it reaches (at
// least K), with one decimal place (e.g. 787.6M, or 0.1K for 100 bytes)
function extfileinfo_byte_estimate(bytes)
{
	var units = ["P", "T", "G", "M", "K"];
	for (var i = 0; i < units.length; i++) {
		var unit = Math.pow(1024, units.length - i);
		if (bytes >= unit)
			return (bytes / unit).toFixed(1) + units[i];
	}
	return (bytes / 1024).toFixed(1) + "K";
}

// A file's date/time from FileBase.get_time() (which reports a missing
// file's -1 as an unsigned number), or 0 for none
function extfileinfo_file_time(t)
{
	return (t > 0 && t !== 0xffffffff) ? t : 0;
}

// What to print for text.dat string 'fmt' with arguments 'args', as the
// built-in display prints it: a string with no % as is, with its @-codes
// expanded (atcodes: true); otherwise formatted, its @-codes left alone
// (they could have come from a user, in the arguments)
function extfileinfo_text(fmt, args)
{
	if (fmt.indexOf("%") < 0)
		return { str: fmt, atcodes: true };
	return { str: format.apply(null, [fmt].concat(args)), atcodes: false };
}

// The color to highlight what a search found in: the found_attr option
// (Ctrl-A codes), or by default bright yellow, a color the field labels and
// values (in the user theme colors, \1u and \1U) don't use
function extfileinfo_found_attr(option)
{
	return option || "\x01n\x01Y\x01H";
}

this;
