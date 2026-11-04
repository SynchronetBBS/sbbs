/* Native, authority-owned IRCServices S-lines. GPL-2.0-or-later. */
"use strict";

var Services_Lines = [];

function IRC_has_slines(peer) {
	return !!(peer && peer.their_capab && peer.their_capab.indexOf("SLINES") >= 0);
}

function IRC_sline_mask(type, value) {
	if (typeof value != "string" || !value.length || value.length > 200 || /[\x00-\x1f\x7f]/.test(value)) return null;
	if (type == "Q" && (/[\s!@,:]/.test(value) || value[0] == "#" || value[0] == "&")) return null;
	/* IRC strings contain wire bytes, including UTF-8 octets. Unicode case
	   conversion would corrupt high bytes (for example C3 becomes E3). */
	return value.replace(/\*+/g, "*").replace(/[A-Z]/g, function(c) {
		return String.fromCharCode(c.charCodeAt(0) + 32);
	});
}

function IRC_match_sline(type, value) {
	for (var i = 0; i < Services_Lines.length; i++) {
		var line = Services_Lines[i];
		if (line.type == type && ULine_Exists(line.owner) && wildmatch(value, line.mask)) return line;
	}
	return null;
}

function IRC_sline_wire(line, remove) {
	if (remove) return "UNS" + line.type + "LINE :" + line.mask;
	return line.type == "G" ? "SGLINE " + line.mask.length + " :" + line.mask + ":" + line.reason
		: "SQLINE " + line.mask + " :" + line.reason;
}

function IRC_receive_sline(peer, origin, verb, p) {
	if (!origin.uline) return;
	var type = verb.indexOf("G") >= 0 ? "G" : "Q", remove = verb.indexOf("UN") == 0;
	var mask, reason = "Reserved";
	if (remove) {
		/* IRCServices uses exact removals. Bahamut's optional wildcard
		   deletion form deliberately has no authority over another entry. */
		if (p.length != 1) return;
		mask = p[0];
	} else if (type == "G") {
		if (p.length != 2 || !/^[0-9]+$/.test(p[0])) return;
		var length = Number(p[0]);
		if (!length || length > 200 || p[1][length] != ":") return;
		mask = p[1].slice(0, length); reason = p[1].slice(length + 1) || reason;
	} else {
		if (p.length < 1 || p.length > 2) return;
		mask = p[0]; reason = p[1] || reason;
	}
	mask = IRC_sline_mask(type, mask);
	if (!mask || /[\x00\r\n]/.test(reason)) return;
	var owner = IRC_akill_owner(origin), line = {type:type, owner:owner, mask:mask, reason:reason};
	/* Keep a whole replayable line, including the original authority, inside
	   the IRC framing limit. Never truncate the mask or invent a new expiry. */
	var wire = IRC_sline_wire(line, remove);
	if ((":" + owner + " " + wire).length > 510) return;
	var found = -1;
	for (var i = 0; i < Services_Lines.length; i++) {
		var old = Services_Lines[i];
		if (old.owner == owner && old.type == type && old.mask == mask) { found = i; break; }
	}
	if (remove) { if (found >= 0) Services_Lines.splice(found, 1); }
	else if (found >= 0) Services_Lines[found] = line;
	else Services_Lines.push(line);
	/* Relay even a redundant correction. Incapable native peers cannot
	   enforce or relay this dialect, and receive ordinary KILLs if needed. */
	for (var name in Local_Servers) {
		var next = Local_Servers[name];
		if (next !== peer && next.admitted && IRC_has_slines(next)) next.originatorout(wire, origin);
	}
}

function IRC_reconcile_slines() {
	/* These wire formats have no expiry or complete-inventory marker. End
	   the runtime grant when its authority disappears; on reconnect the
	   services database republishes active policy. Local Q-lines are separate. */
	Services_Lines = Services_Lines.filter(function(line) { return ULine_Exists(line.owner) && !!Servers[line.owner]; });
}

function IRC_burst_slines(peer) {
	if (!IRC_has_slines(peer)) return;
	for (var i = 0; i < Services_Lines.length; i++) {
		var line = Services_Lines[i], owner = Servers[line.owner];
		if (!owner || !owner.uline || owner === peer || owner.parent == peer.nick) continue;
		peer.rawout(":" + owner.nick + " " + IRC_sline_wire(line, false));
	}
}

function IRC_sline_introduction(peer, user) {
	if (user.uline || user.services_internal) return false;
	var line = IRC_match_sline("Q", user.nick) || IRC_match_sline("G", user.realname);
	if (!line) return false;
	peer.ircout("KILL " + user.nick + " :S" + line.type + "LINE: " + line.reason);
	return true;
}
