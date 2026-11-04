/* Channel exception syntax shared by the daemon and embedded services.
 * GPL-2.0-or-later. No daemon state is needed to normalize an entry. */
"use strict";

var IRC_MAX_EXCEPTIONS = 25;
var IRC_MAX_INVITEMASKS = 25;

function IRC_joinrate_parse(value) {
	var match = /^([0-9]{1,10})(?::([0-9]{1,10}))?$/.exec(String(value));
	if (!match) return null;
	var count = Number(match[1]), seconds = Number(match[2] || 0);
	if (count > 2147483647 || seconds > 2147483647 || (count && match[2] === undefined)) return null;
	if (!count || !seconds) return {value:"0", count:0, seconds:0};
	return {value:count + ":" + seconds, count:count, seconds:seconds};
}

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

/* Authority is independent of dialect. CHANEXCEPT explicitly negotiates
 * extended masks; a configured Bahamut peer understands ordinary +e only. */
function IRC_exception_support(peer) {
	return IRC_has_chanexcept(peer) ? true : peer && peer.protocol == "bahamut" ? "ban" : false;
}

function IRC_extended_modes(peer) {
	return {invite:!!(peer && (peer.protocol == "bahamut"
		|| peer.their_capab && peer.their_capab.indexOf("INVMASK") >= 0)),
		policy:!!(peer && (peer.protocol == "bahamut"
		|| peer.their_capab && peer.their_capab.indexOf("CHANPOLICY") >= 0))};
}

/* Keep the complete native/Bahamut argument grammar when filtering modes
 * for an older peer. Removing one must never shift the following arguments.
 * null means the grammar is unknown and the request must be rejected. */
function IRC_chanmode_argc(mode, add) {
	if ("beIkov".indexOf(mode) >= 0) return 1;
	if (mode == "l" || mode == "j") return add ? 1 : 0;
	return "imnpstrRcOMS".indexOf(mode) >= 0 ? 0 : null;
}

function IRC_chanmode_valid(fields) {
	if (!fields || !fields[0]) return false;
	var add = true, index = 1;
	for (var i = 0; i < fields[0].length; i++) {
		var mode = fields[0][i];
		if (mode == "+" || mode == "-") { add = mode == "+"; continue; }
		var count = IRC_chanmode_argc(mode, add);
		if (count === null || (count && fields[index++] === undefined)) return false;
	}
	return true;
}

/* Consume arguments before removing unsupported operations. An unknown
 * mode must never shift the arguments of the known modes that follow it. */
function IRC_exception_filter_modes(fields, supported, extended) {
	if (!IRC_chanmode_valid(fields)) return null;
	var modes = "", args = [], add = true, emitted, index = 1;
	for (var i = 0; i < fields[0].length; i++) {
		var mode = fields[0][i];
		if (mode == "+" || mode == "-") { add = mode == "+"; continue; }
		var takes_arg = IRC_chanmode_argc(mode, add);
		if (takes_arg === null) return null;
		var arg = takes_arg ? fields[index++] : undefined;
		if (takes_arg && arg === undefined) return null;
		if ("beiklmnpstov".indexOf(mode) < 0
			&& !(mode == "I" && extended && extended.invite)
			&& !("rRMOcjS".indexOf(mode) >= 0 && extended && extended.policy)) continue;
		if (mode == "e" && !supported) continue;
		if (mode == "e" && supported == "ban") {
			var exception = IRC_exception_parse(arg);
			if (!exception || exception.type != "ban") continue;
			arg = exception.value;
		}
		if (emitted !== add) { modes += add ? "+" : "-"; emitted = add; }
		modes += mode;
		if (takes_arg && arg !== undefined) args.push(arg);
	}
	return modes ? [modes].concat(args) : null;
}

function IRC_chanmode_batches(fields, frame, limit) {
	fields = IRC_exception_filter_modes(fields, true, {invite:true, policy:true});
	if (!fields) return [];
	var batches = [], modes = "", args = "", add = true, emitted, count = 0, index = 1;
	for (var i = 0; i < fields[0].length; i++) {
		var mode = fields[0][i];
		if (mode == "+" || mode == "-") { add = mode == "+"; continue; }
		var takes_arg = IRC_chanmode_argc(mode, add);
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
