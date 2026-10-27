/* Optional, hub-vouched observations of live server links. GPL-2.0-or-later.
 * These are reports,
 * not cryptographic proof of the security of a remote server. */

function IRC_has_linksec(peer) {
	return peer && peer.their_capab && peer.their_capab.indexOf("LINKSEC") >= 0;
}

/* Store both endpoints' observations on the child object. Removing that
 * subtree (including a same-name reconnect) removes all of its link state. */
function IRC_linksec_edge(a, b) {
	a = String(a).toLowerCase(); b = String(b).toLowerCase();
	var sa = Servers[a], sb = Servers[b], local = ServerName.toLowerCase();
	if (a == b || (a != local && (!sa || !sa.server)) || (b != local && (!sb || !sb.server))) return null;
	if (sa && String(sa.linkparent).toLowerCase() == b) return sa;
	if (sb && String(sb.linkparent).toLowerCase() == a) return sb;
	return null;
}

function IRC_linksec_equal(a, b) {
	return a.transport == b.transport && a.direction == b.direction && a.validation == b.validation;
}

function IRC_linksec_store(edge, report) {
	var side = report.reporter == edge.nick.toLowerCase() ? "child" : "parent";
	if (!edge.link_security) edge.link_security = {};
	var slot = edge.link_security[side];
	if (!slot) {
		edge.link_security[side] = {first:report};
		return true;
	}
	if (IRC_linksec_equal(slot.first, report) || (slot.conflict && IRC_linksec_equal(slot.conflict, report))) return false;
	/* An admitted TCP link cannot change transport or direction. Retain
	   contradictory evidence, bounded to two records per endpoint, and keep
	   any plaintext evidence when an endpoint changes its story again. */
	if (!slot.conflict || (report.transport == "PLAIN" && slot.first.transport != "PLAIN" && slot.conflict.transport != "PLAIN")) {
		slot.conflict = report;
		return true;
	}
	return false;
}

function IRC_linksec_send(peer, report) {
	if (!peer.admitted || peer.quitting || !IRC_has_linksec(peer)) return;
	/* Do not reflect a source prefix back down its own branch. */
	var source = Servers[report.reporter];
	if (report.reporter == peer.nick.toLowerCase()
		|| (source && source.parent && String(source.parent).toLowerCase() == peer.nick.toLowerCase())) return;
	peer.rawout(":" + report.reporter + " LINKSEC " + report.neighbor + " "
		+ report.transport + " " + report.direction + " " + report.validation);
}

function IRC_linksec_publish(edge, report, except) {
	if (!IRC_linksec_store(edge, report)) return;
	for (var name in Local_Servers) {
		var peer = Local_Servers[name];
		if (peer !== except) IRC_linksec_send(peer, report);
	}
}

function IRC_linksec_admit(peer) {
	if (!peer.socket || !peer.admitted) return;
	var tls = !!peer.socket.ssl_session, outbound = !!peer.socket.outbound;
	IRC_linksec_publish(peer, {reporter:ServerName.toLowerCase(), neighbor:peer.nick.toLowerCase(),
		transport:tls ? "TLS" : "PLAIN", direction:outbound ? "OUT" : "IN",
		validation:tls && outbound ? (peer.socket.irc_tls_verified === true ? "VERIFIED" : "UNVERIFIED") : "NA"}, peer);
}

function IRC_linksec_internal(child) {
	IRC_linksec_publish(child, {reporter:ServerName.toLowerCase(), neighbor:child.nick.toLowerCase(),
		transport:"INTERNAL", direction:"NONE", validation:"NA"});
}

function IRC_receive_linksec(peer, origin, p) {
	/* A U:Line is not permission to vouch for network security. Leaves may
	   use their own socket observations locally, but cannot upgrade reports
	   elsewhere, including the certificate checks on their own uplink. */
	if (!peer.admitted || !peer.hub || !IRC_has_linksec(peer) || !origin.server || p.length != 4) return;
	if (origin !== peer && (origin.local || !origin.parent
		|| String(origin.parent).toLowerCase() != peer.nick.toLowerCase())) return;
	var edge = IRC_linksec_edge(origin.nick, p[0]);
	if (!edge) return;
	var transport = p[1], direction = p[2], validation = p[3];
	if (transport == "INTERNAL") {
		if (direction != "NONE" || validation != "NA" || origin.nick.toLowerCase() == edge.nick.toLowerCase()) return;
	} else {
		if ((transport != "TLS" && transport != "PLAIN") || (direction != "OUT" && direction != "IN")) return;
		if (transport == "TLS" && direction == "OUT") {
			if (validation != "VERIFIED" && validation != "UNVERIFIED") return;
		} else if (validation != "NA") return;
	}
	IRC_linksec_publish(edge, {reporter:origin.nick.toLowerCase(), neighbor:p[0].toLowerCase(),
		transport:transport, direction:direction, validation:validation}, peer);
}

function IRC_linksec_burst(peer) {
	if (!IRC_has_linksec(peer)) return;
	for (var name in Servers) {
		var edge = Servers[name];
		if (edge.local && !edge.admitted) continue;
		var state = edge.link_security;
		if (!state) continue;
		for (var side in state) {
			IRC_linksec_send(peer, state[side].first);
			if (state[side].conflict) IRC_linksec_send(peer, state[side].conflict);
		}
	}
}

function IRC_linksec_status(edge) {
	var state = edge.link_security || {}, records = [], conflict = false;
	for (var side in state) {
		records.push(state[side].first);
		if (state[side].conflict) { records.push(state[side].conflict); conflict = true; }
	}
	var result = {transport:"UNKNOWN", validation:"UNKNOWN", checker:"", conflict:conflict};
	for (var i = 0; i < records.length; i++) {
		var r = records[i];
		if (i && (records[0].transport != r.transport || records[0].direction == r.direction)) conflict = true;
		if (r.transport == "PLAIN") result.transport = "PLAIN";
	}
	result.conflict = conflict;
	if (conflict) return result;
	if (!records.length) return result;
	result.transport = records[0].transport;
	if (result.transport != "TLS") { result.validation = "NA"; return result; }
	for (i = 0; i < records.length; i++) if (records[i].direction == "OUT") {
		result.validation = records[i].validation;
		result.checker = records[i].reporter;
	}
	return result;
}

function IRC_do_tlsroute(user, target) {
	if (!target) { user.numeric461("TLSROUTE"); return; }
	var name = target.toLowerCase(), local = ServerName.toLowerCase();
	if (name != local && (!Servers[name] || !Servers[name].server || (Servers[name].local && !Servers[name].admitted))) {
		user.numeric402(target); return;
	}
	var path = [], seen = {}, broken = false;
	while (name != local) {
		var edge = Servers[name];
		if (!edge || !edge.server || seen["=" + name] || !edge.linkparent) { broken = true; break; }
		seen["=" + name] = true;
		path.push(edge); name = edge.linkparent.toLowerCase();
	}
	user.server_notice("TLSROUTE " + ServerName + " -> " + target);
	var plain = false, unknown = broken, unverified = false, validation_unknown = broken, physical = 0;
	if (!broken) for (var i = path.length - 1; i >= 0; i--) {
		edge = path[i];
		var status = IRC_linksec_status(edge), check = status.validation;
		if (status.checker) check += " by " + status.checker;
		if (status.conflict) check += " (conflicting reports)";
		user.server_notice("TLSROUTE " + edge.linkparent + " -> " + edge.nick + ": "
			+ status.transport + "; certificate validation " + check);
		if (status.transport == "INTERNAL") continue;
		physical++;
		if (status.transport == "PLAIN") plain = true;
		if (status.transport == "UNKNOWN") unknown = true;
		if (status.validation == "UNVERIFIED") unverified = true;
		if (status.validation == "UNKNOWN") validation_unknown = true;
	}
	var summary = plain ? "plaintext present" : unknown ? "unknown" : physical ? "all network hops TLS" : "no network hops";
	var validation = plain || (!physical && !broken) ? "not applicable"
		: unverified ? "unverified" : validation_unknown ? "unknown" : "verified";
	user.server_notice("TLSROUTE END " + target + ": " + summary + "; certificate validation " + validation);
}
