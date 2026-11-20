// Converts the additional Internet domain list of Synchronet v3.21 and
// older (ctrl/domains.cfg, one domain name or pattern per line) into
// [domain:<name>] sections of ctrl/msgs.ini, as used by v3.22 and later.
// Called by update.js:
//
// require("domains_cfg_upgrade.js", "upgrade_domains_cfg");
// var result = upgrade_domains_cfg(system.ctrl_dir [, dry_run]);
//
// domains.cfg itself is never renamed or removed: a server still running
// an older binary (not yet restarted, or on another host sharing this
// ctrl directory) reads it for every message. Instead the key
// domains_converted = true in the [Internet] section of msgs.ini records
// that the list now lives there; the configuration loader then never
// reads domains.cfg, so an emptied list stays empty. SCFG sets the same
// key whenever it saves msgs.ini.
//
// Nothing is done when domains.cfg does not exist or has no entries
// (only comments or blank lines), or when msgs.ini already has the key
// (so running this twice changes nothing); a missing msgs.ini is an
// error and nothing is created. When msgs.ini already has a [domain:]
// section the file is not converted: if every one of its entries is
// already a section (as after an older SCFG saved the list it read from
// domains.cfg) the key is set ("marked"), otherwise nothing is changed
// and a warning is given ("skipped"). Otherwise each non-blank,
// non-comment line becomes a section, in file order, with
// send_from = false and a blank send_ars (so a converted system behaves
// as before), all appended to msgs.ini in one write, and then the key is
// set (in that order: an interruption leaves the sections without the
// key, which the loader reads just the same). The key is set with the
// ini library, which writes the file as SCFG does (LF line endings on
// Unix). A list that ends with a
// negated ('!') entry is converted as it is, with a warning.
//
// The file is read so that the list matches every host exactly as
// findstr() on the file did, by the same rules by which the
// configuration loader reads it as a fallback while msgs.ini has no
// [domain:] section (read_domains_cfg_file() in src/sbbs3/scfglib1.c:
// keep the two in agreement):
// - a line is what fgets() reads into a 1001-byte buffer (at most 1000
//   characters; a longer line continues as the next "line");
// - leading white space (IS_WHITESPACE: space, tab, CR, LF, FF, VT and
//   0xFF) is skipped and an empty line is not evaluated;
// - C escapes are interpreted (c_unescape_str(): \xHH, \DDD decimal,
//   \e \a \b \f \n \r \t \v), the text from the first tab is cut and
//   trailing white space is removed;
// - a line that is then empty or begins with ';' is inert, as is a later
//   case-insensitive duplicate of an earlier line; inert lines are
//   dropped, except that the last line is kept when its value for a host
//   that no line decides ("no match" for a comment or a duplicate of a
//   plain entry, "match" for a duplicate of a negated entry) differs from
//   that of the entry that would otherwise be last (a comment as is, an
//   empty line as ";").
// A name that cannot be a section name (256 characters or more,
// containing CR or LF, or beginning with white space after unescaping)
// is an inert ";" entry (with a warning here): that is exact only for
// host names without such characters. At most 10000 lines (fgets()
// pieces, blank ones included) are read, with a warning here.
//
// Returns an object: { status, domains, warnings }, where status is one
// of "none" (no domains.cfg, no entries, or already converted), "skipped"
// or "marked" (msgs.ini already has [domain:] sections), "converted" or
// "error"
// (with a message property), domains is the array of names converted (or
// that would be, when dry_run is true) and warnings an array of strings.

"use strict";

// As DOMAINS_CFG_MAX_LINES in src/sbbs3/scfglib1.c (fgets() pieces, blank
// lines included)
var DOMAINS_CFG_MAX_LINES = 10000;

function domains_cfg_is_space(ch)
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v'
		|| ch == '\xFF';
}

// As c_unescape_str() (decimal \DDD, 2-digit \xHH, char arithmetic mod 256);
// a resulting NUL ends the string
function domains_cfg_unescape(str)
{
	var out = "";
	var i = 0;
	var map = { e: 27, a: 7, b: 8, f: 12, n: 10, r: 13, t: 9, v: 11 };

	while (i < str.length) {
		var ch = str.charCodeAt(i++);
		if (ch == 0x5C) {	// backslash
			var c = str.charAt(i);
			var digits = 0;
			if (c == 'x') {
				i++;
				ch = 0;
				while (digits < 2 && i < str.length && /[0-9A-Fa-f]/.test(str.charAt(i))) {
					ch = (ch * 16 + parseInt(str.charAt(i), 16)) & 0xFF;
					i++;
					digits++;
				}
			} else if (/[0-9]/.test(c)) {
				ch = 0;
				while (digits < 3 && i < str.length && /[0-9]/.test(str.charAt(i))) {
					ch = (ch * 10 + (str.charCodeAt(i) & 0xF)) & 0xFF;
					i++;
					digits++;
				}
			} else {
				ch = (c == "") ? 0 : (map[c] !== undefined ? map[c] : c.charCodeAt(0));
				i++;
			}
		}
		if (ch == 0)
			break;
		out += String.fromCharCode(ch);
	}
	return out;
}

// The entries of a domains.cfg file's contents (a byte string), by the
// rules in the comment at the top of this file
function domains_cfg_entries(text)
{
	var entries = [];
	var warnings = [];
	var pos = 0;
	var have_last = false;
	var last_inert = false;
	var last_value = false;
	var last_text = "";
	var lines = 0;

	function storable(name) {
		return name.length < 256 && !/[\r\n]/.test(name) && !domains_cfg_is_space(name.charAt(0));
	}

	while (pos < text.length) {
		if (++lines > DOMAINS_CFG_MAX_LINES) {
			warnings.push("Only the first " + DOMAINS_CFG_MAX_LINES + " lines were read");
			break;
		}
		// fgets() into a 1001-byte buffer
		var end = text.indexOf("\n", pos);
		end = (end < 0) ? text.length : end + 1;
		if (end - pos > 1000)
			end = pos + 1000;
		var line = text.substring(pos, end);
		pos = end;
		var nul = line.indexOf("\0");
		if (nul >= 0)
			line = line.substr(0, nul);

		var i = 0;
		while (i < line.length && domains_cfg_is_space(line.charAt(i)))
			i++;
		var name = line.substr(i);
		if (name.length < 1)
			continue;
		name = domains_cfg_unescape(name);
		var tab = name.indexOf("\t");
		if (tab >= 0)
			name = name.substr(0, tab);
		var len = name.length;
		while (len > 0 && domains_cfg_is_space(name.charAt(len - 1)))
			len--;
		name = name.substr(0, len);

		var inert = (name.length < 1 || name.charAt(0) == ';');
		var value = false;	// for a host that no line decides
		if (!inert && !storable(name)) {
			warnings.push("Not convertible exactly (kept as an inert entry): " + JSON.stringify(name));
			inert = true;
			name = ";";
		}
		if (!inert) {
			var key = name.toLowerCase();
			for (var k = 0; k < entries.length; k++) {
				if (entries[k].toLowerCase() == key) {
					inert = true;
					value = (entries[k].charAt(0) == '!');
					break;
				}
			}
		}
		have_last = true;
		last_inert = inert;
		last_value = value;
		last_text = (name.length < 1 || !storable(name)) ? ";" : name;
		if (!inert) {
			if (name.indexOf("\\") >= 0)
				warnings.push("Backslash in a domain name: " + name);
			entries.push(name);
		}
	}
	// Keep a last inert line when its value differs from that of the last
	// entry kept (an empty list or a plain entry: "no match"; a negated
	// entry: "match")
	if (have_last && last_inert) {
		var kept_value = entries.length > 0 && entries[entries.length - 1].charAt(0) == '!';
		if (kept_value != last_value)
			entries.push(last_text);
	}
	return { entries: entries, warnings: warnings };
}

function upgrade_domains_cfg(ctrl_dir, dry_run)
{
	var cfg_path = ctrl_dir + "domains.cfg";
	var ini_path = ctrl_dir + "msgs.ini";
	var result = { status: "none", domains: [], warnings: [] };
	var i;

	if (!file_exists(cfg_path))
		return result;

	var cfg = new File(cfg_path);
	if (!cfg.open("rb")) {
		result.status = "error";
		result.message = "Error " + cfg.error + " opening " + cfg_path;
		return result;
	}
	var parsed = domains_cfg_entries(cfg.read());
	cfg.close();
	result.domains = parsed.entries;
	result.warnings = parsed.warnings;

	// A file with no entries (e.g. only comments) is left alone
	if (result.domains.length < 1)
		return result;

	if (!file_exists(ini_path)) {
		result.status = "error";
		result.message = ini_path + " does not exist";
		return result;
	}
	var ini = new File(ini_path);
	if (!ini.open("rb")) {
		result.status = "error";
		result.message = "Error " + ini.error + " opening " + ini_path;
		return result;
	}
	var converted = ini.iniGetValue("Internet", "domains_converted", false);
	var sections = ini.iniGetSections("domain:");
	ini.rewind();
	var text = ini.read();
	ini.close();
	if (converted) {
		result.domains = [];
		result.warnings = [];
		return result;
	}
	if (sections.length > 0) {
		var have = {};
		sections.forEach(function (section) {
			have[section.substr(7).replace(/^[ \t\r\n\f\v\xFF]+/, "").toLowerCase()] = true;
		});
		var missing = result.domains.filter(function (name) { return !have[name.toLowerCase()]; });
		result.domains = [];
		if (missing.length > 0) {
			result.status = "skipped";
			result.warnings = [ini_path + " already has [domain:] sections, so "
				+ cfg_path + " is not used; not in msgs.ini: " + missing.join(", ")];
			return result;
		}
		result.status = "marked";
		result.warnings = [];
		if (!dry_run)
			set_converted(ini_path, result);
		return result;
	}

	// An unmatched host takes the result of the last line the matcher
	// evaluated: after a trailing '!' entry, every other host is local
	// (as it was with domains.cfg)
	var last = result.domains[result.domains.length - 1];
	if (last.charAt(0) == '!')
		result.warnings.push("The domain list ends with the negated entry " + last
			+ ", so every domain not matched by an earlier entry is treated as local"
			+ " (as it was in domains.cfg): move it above the wildcard entry it makes an exception to");

	result.status = "converted";
	if (dry_run)
		return result;

	// Append all of the sections with a single write
	var eol = (text.indexOf("\r\n") >= 0) ? "\r\n" : "\n";
	var add = "";
	if (text.length > 0 && text.charAt(text.length - 1) != "\n")
		add += eol;
	for (i = 0; i < result.domains.length; i++)
		add += "[domain:" + result.domains[i] + "]" + eol
			+ "\tsend_from=false" + eol
			+ "\tsend_ars=" + eol;
	ini = new File(ini_path);
	if (!ini.open("ab") || !ini.write(add)) {
		result.status = "error";
		result.message = "Error " + ini.error + " writing " + ini_path;
		ini.close();
		return result;
	}
	ini.close();
	set_converted(ini_path, result);
	return result;
}

// Records in msgs.ini that the additional domains are kept there
function set_converted(ini_path, result)
{
	var ini = new File(ini_path);
	if (!ini.open("r+") || !ini.iniSetValue("Internet", "domains_converted", true)) {
		result.status = "error";
		result.message = "Error " + ini.error + " writing " + ini_path;
	}
	ini.close();
}

this;
