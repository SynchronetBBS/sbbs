/* Channel exception syntax shared by the daemon and embedded services.
 * GPL-2.0-or-later. No daemon state is needed to normalize an entry. */
"use strict";

var IRC_MAX_EXCEPTIONS = 25;

function IRC_exception_parse(value) {
	if (typeof value != "string" || !value.length || /[^\x21-\x7e]|,/.test(value)) return null;
	var type = "ban", mask = value, prefix = "", ext;
	if (/^~[^!@]*:/.test(value)) {
		ext = /^~(?:msgbypass|m):(external|moderated):(.+)$/i.exec(value);
		if (!ext) return null;
		type = ext[1].toLowerCase(); mask = ext[2]; prefix = "~msgbypass:" + type + ":";
		if (/^~[^!@]*:/.test(mask)) return null;
	}
	var nick = "*", user = "*", host = "*", bang = mask.indexOf("!"), at = mask.indexOf("@");
	if (bang >= 0) {
		if (mask.indexOf("!", bang + 1) >= 0 || (at >= 0 && at < bang)) return null;
		nick = mask.slice(0, bang) || "*";
		user = mask.slice(bang + 1, at < 0 ? mask.length : at) || "*";
		if (at >= 0) host = mask.slice(at + 1) || "*";
	} else if (at >= 0) {
		user = mask.slice(0, at) || "*"; host = mask.slice(at + 1) || "*";
	} else if (/[.:]/.test(mask)) host = mask;
	else nick = mask;
	if (/[!@]/.test(nick + user + host)) return null;
	mask = (nick + "!" + user + "@" + host).replace(/\*+/g, "*");
	var canonical = prefix + mask;
	/* Leave enough space for a channel name, source and numeric framing.
	   The daemon also checks its actual MODE and list-reply framing. */
	if (nick.length > 30 || canonical.length > 220) return null;
	return {type:type, mask:mask, value:canonical};
}

function IRC_has_chanexcept(peer) {
	return !!(peer && peer.their_capab && peer.their_capab.indexOf("CHANEXCEPT") >= 0);
}

/* Consume arguments before removing unsupported operations. An unknown
 * mode must never shift the arguments of the known modes that follow it. */
function IRC_exception_filter_modes(fields, supported) {
	var modes = "", args = [], add = true, emitted, index = 1;
	for (var i = 0; i < fields[0].length; i++) {
		var mode = fields[0][i];
		if (mode == "+" || mode == "-") { add = mode == "+"; continue; }
		var takes_arg = "bekov".indexOf(mode) >= 0 || (mode == "l" && add);
		var arg = takes_arg ? fields[index++] : undefined;
		if (mode == "e" && !supported) continue;
		if (emitted !== add) { modes += add ? "+" : "-"; emitted = add; }
		modes += mode;
		if (takes_arg && arg !== undefined) args.push(arg);
	}
	return modes ? [modes].concat(args) : null;
}

function IRC_chanmode_batches(fields, frame, limit) {
	var batches = [], modes = "", args = "", add = true, emitted, count = 0, index = 1;
	for (var i = 0; i < fields[0].length; i++) {
		var mode = fields[0][i];
		if (mode == "+" || mode == "-") { add = mode == "+"; continue; }
		var takes_arg = "bekov".indexOf(mode) >= 0 || (mode == "l" && add);
		var arg = takes_arg ? " " + fields[index++] : "";
		var sign = emitted === add ? "" : add ? "+" : "-";
		if (count && (count >= limit || (frame + modes + sign + mode + args + arg).length > 510)) {
			batches.push(modes + args); modes = args = ""; count = 0; emitted = undefined;
			sign = add ? "+" : "-";
		}
		modes += sign + mode; args += arg; emitted = add; count++;
	}
	if (count) batches.push(modes + args);
	return batches;
}
