// Color (attribute) options given as attribute letters, as in attr.ini
// (e.g. "WH4" for bright white on blue), for modules' modopts.ini options

// Usage: require("attr_option.js", "attr_option");

// Ctrl-A codes for a color option's value: its attribute letters (K, R, G, Y,
// B, M, C and W for the foreground, 0 to 7 for the background, H for high
// intensity, I for blink, E for a bright background, U/u and V/v for the
// user and mnemonic colors), up to the first character that isn't one;
// "" for no letters. A value with Ctrl-A codes is returned unchanged.
function attr_option(value)
{
	if (typeof value === "number")	// (modopts.js converts all-digit values)
		value = String(value);
	if (typeof value !== "string")
		return "";
	if (value.indexOf("\x01") >= 0)	// Ctrl-A codes, as these options once took
		return value;
	var letters = value.match(/^[KRGYBMCWHIEUV0-7]*/i)[0];
	if (letters === "")
		return "";
	return "\x01n" + letters.replace(/(.)/g, "\x01$1");
}

// The options with each color option's value (those named *_attr) as
// Ctrl-A codes
function attr_options(opts)
{
	var result = {};
	for (var key in opts)
		result[key] = /_attr$/.test(key) ? attr_option(opts[key]) : opts[key];
	return result;
}

this;
