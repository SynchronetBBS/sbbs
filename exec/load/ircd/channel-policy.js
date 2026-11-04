/* Extended channel policies. GPL-2.0-or-later. */
"use strict";

function IRC_joinrate_bucket(chan) {
	if (!(chan.mode & CHANMODE_JOINRATE)) return null;
	var rate = IRC_joinrate_parse(chan.arg[CHANMODE_JOINRATE]);
	if (!rate || !rate.count) return null;
	var state = chan.joinrate;
	if (!state || state.value != rate.value)
		state = chan.joinrate = {value:rate.value, tokens:rate.count, time:system.timer};
	state.tokens = Math.min(rate.count, state.tokens + Math.max(0, system.timer - state.time) * rate.count / rate.seconds);
	state.time = system.timer;
	state.count = rate.count;
	return state;
}

function IRC_channel_join_policy(user, chan, check_rate) {
	if (user.uline || IRC_is_internal_service(user)) return null;
	if ((chan.mode & CHANMODE_REGISTEREDONLY) && !IRC_services_identified(user))
		return {numeric:477, reason:"Cannot join channel (+R: identify to services first)"};
	if ((chan.mode & CHANMODE_OPERONLY) && !(user.mode & USERMODE_OPER))
		return {numeric:520, reason:"Cannot join channel (+O: IRC operators only)"};
	if ((chan.mode & CHANMODE_SECURE) && IRC_client_tls_status(user) != "TLS")
		return {numeric:489, reason:"Cannot join channel (+S: known TLS client connection required)"};
	var bucket = check_rate && IRC_joinrate_bucket(chan);
	if (bucket && bucket.tokens < 1)
		return {numeric:471, reason:"Cannot join channel (+j: join rate exceeded)"};
	return null;
}

function IRC_channel_joined(user, chan) {
	if (user.uline || IRC_is_internal_service(user)) return;
	var bucket = IRC_joinrate_bucket(chan);
	/* Remote admissions consume capacity too. Bound distributed debt so a
	   burst cannot make an otherwise usable channel unavailable indefinitely. */
	if (bucket) bucket.tokens = Math.max(-bucket.count, bucket.tokens - 1);
}

function IRC_channel_remote_join(peer, user, chan) {
	var denied = IRC_channel_join_policy(user, chan, !peer.bursting);
	if (!denied) return true;
	user.numeric(denied.numeric, chan.nam + " :" + denied.reason);
	/* The remote daemon has already accepted its JOIN. Correct that branch;
	   the rejected membership must never enter the canonical channel. */
	var previous = peer.rejected_join;
	peer.rejected_join = {channel:chan, user:user};
	try { peer.ircout("KICK " + chan.nam + " " + user.nick + " :" + denied.reason); }
	finally {
		if (previous === undefined) delete peer.rejected_join;
		else peer.rejected_join = previous;
	}
	return false;
}

function IRC_channel_has_controls(message) {
	for (var i = 0; i < message.length; i++) {
		var code = message.charCodeAt(i);
		if (code > 31 || code == 1) continue; // CTCP framing is allowed.
		if (code == 27 && (message[i + 1] == "$" || message[i + 1] == "(")) { i++; continue; }
		return true;
	}
	return false;
}

function IRC_peer_chanmode(peer, chan) {
	var fields = IRC_exception_filter_modes(chan.chanmode(true).split(" "), IRC_exception_support(peer), IRC_extended_modes(peer));
	return fields ? fields.join(" ") : "+";
}
