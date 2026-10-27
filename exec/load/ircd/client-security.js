/* First-party client transport observations. GPL-2.0-or-later.
 * Only the client's owning server can report its socket state. This grants
 * no authority over another server's clients or inter-server link security. */

function IRC_has_clienttls(peer) {
	return peer && peer.their_capab && peer.their_capab.indexOf("CLIENTTLS") >= 0;
}

function IRC_client_tls_status(user) {
	if (!user) return "UNKNOWN";
	if (user.services_internal) return "INTERNAL";
	if (user.local) {
		if (!user.socket || typeof user.socket.ssl_session != "boolean") return "UNKNOWN";
		return user.socket.ssl_session ? "TLS" : "PLAIN";
	}
	var state = user.client_tls;
	if (!state) return "UNKNOWN";
	if (state.first == "PLAIN" || state.conflict == "PLAIN") return "PLAIN";
	return state.conflict ? "UNKNOWN" : state.first;
}

function IRC_client_tls_send(peer, user, transport) {
	if (!peer.admitted || peer.quitting || !IRC_has_clienttls(peer)) return;
	if (user.parent && String(user.parent).toLowerCase() == peer.nick.toLowerCase()) return;
	peer.rawout(":" + user.servername.toLowerCase() + " CLIENTTLS " + user.nick + " " + transport);
}

function IRC_client_tls_burst(peer, user) {
	if (user.local || user.services_internal) {
		var status = IRC_client_tls_status(user);
		if (status != "UNKNOWN") IRC_client_tls_send(peer, user, status);
	} else if (user.client_tls) {
		IRC_client_tls_send(peer, user, user.client_tls.first);
		if (user.client_tls.conflict) IRC_client_tls_send(peer, user, user.client_tls.conflict);
	}
}

function IRC_receive_clienttls(peer, origin, p) {
	if (!peer.admitted || !IRC_has_clienttls(peer) || !origin.server || p.length != 2) return;
	if (origin !== peer && (origin.local || !origin.parent || String(origin.parent).toLowerCase() != peer.nick.toLowerCase())) return;
	var user = Users[p[0].toUpperCase()], transport = p[1];
	if (!user || user.local || !user.parent || String(user.parent).toLowerCase() != peer.nick.toLowerCase()
		|| user.servername.toLowerCase() != origin.nick.toLowerCase()) return;
	if (["TLS", "PLAIN", "INTERNAL"].indexOf(transport) < 0) return;
	if (transport == "INTERNAL" && !user.uline && user.uprefix != "bbs+ircd+agent") return;
	var state = user.client_tls;
	if (!state) user.client_tls = {first:transport};
	else {
		if (state.first == transport || state.conflict == transport) return;
		if (state.conflict && (transport != "PLAIN" || state.first == "PLAIN" || state.conflict == "PLAIN")) return;
		state.conflict = transport;
	}
	for (var name in Local_Servers) if (Local_Servers[name] !== peer) IRC_client_tls_send(Local_Servers[name], user, transport);
}
