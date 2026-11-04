/* External services identity and the Bahamut/native wire boundary.
 * GPL-2.0-or-later. External identity never proves a Synchronet BBS account.
 */
"use strict";

function IRC_has_svcstate(peer) {
	return !!(peer && peer.their_capab && peer.their_capab.indexOf("SVCSTATE") >= 0);
}

function IRC_external_state(user) {
	var state = user && user.external_services;
	return state && ULine_Exists(state.owner) && state.created == user.created
		&& state.nick == user.nick.toUpperCase() ? state : null;
}

function IRC_services_identified(user) {
	var state = IRC_external_state(user);
	return !!(state && state.identified);
}

function IRC_services_admin(user) {
	var state = IRC_external_state(user);
	return !!(state && state.identified && state.admin && (user.mode & USERMODE_OPER));
}

function IRC_whois_services(viewer, user) {
	if (IRC_services_identified(user)) viewer.numeric(307, user.nick + " :has identified for this nickname.");
	if (IRC_services_admin(user)) viewer.numeric(320, user.nick + " :is a Services administrator.");
}

function IRC_services_rehash_state() {
	for (var name in Users) {
		var user = Users[name], state = user.external_services;
		if (state && !ULine_Exists(state.owner)) {
			delete user.external_services;
			user.services_stamp = 0;
		}
		if (user.services_stamp_owner && !ULine_Exists(user.services_stamp_owner)) {
			user.services_stamp = 0; delete user.services_stamp_owner;
		}
	}
}

function IRC_services_nick_changed(user, nick, created) {
	delete user.leaf_nick_view;
	var state = IRC_external_state(user);
	if (!state) { delete user.external_services; return; }
	if (nick.toUpperCase() != user.nick.toUpperCase()) state.identified = state.admin = false;
	state.nick = nick.toUpperCase(); state.created = created;
}

/* Preserve the leaf's view only as a conditional-message address. It never
   supplies canonical time, authentication, or the BBS session identity. */
function IRC_record_nick_timestamp(user, peer, nick, wire, canonical) {
	delete user.leaf_nick_view;
	if (peer.hub) return;
	wire = IRC_parse_channel_timestamp(wire);
	user.leaf_nick_view = {peer:peer, nick:nick.toUpperCase(), created:canonical,
		wire:wire ? wire : null};
}

function IRC_nick_timestamp_view(user, peer) {
	if (!peer || peer.hub || user.local || String(user.parent).toLowerCase() != peer.nick.toLowerCase())
		return user.created;
	var view = user.leaf_nick_view;
	if (!view || view.peer !== peer || Local_Servers[peer.nick.toLowerCase()] !== peer
		|| view.nick != user.nick.toUpperCase() || view.created != user.created) return null;
	return view.wire;
}

function IRC_services_modes(peer, user) {
	var modes = user.get_usermode(true);
	if (peer.protocol == "bahamut") {
		var state = IRC_external_state(user);
		if (state && state.owner == peer.nick.toLowerCase()) {
			if (IRC_services_identified(user)) modes += "r";
			if (IRC_services_admin(user)) modes += "a";
		}
		if (IRC_client_tls_status(user) == "TLS") modes += "S";
	}
	return modes;
}

function IRC_services_native_modes(peer, modes) {
	/* Bahamut r/d/a/S are not Synchronet's operator notice modes or proof
	   of identification or client TLS. Grants use SVSMODE; TLS is first-party. */
	return peer.protocol == "bahamut" ? modes.replace(/[rdaS]/g, "") : modes;
}

function IRC_services_stamp(peer, user) {
	if (peer.protocol != "bahamut" && !IRC_has_svcstate(peer)) return 0;
	var state = IRC_external_state(user);
	var stamp = state ? state.stamp : user.services_stamp || 0;
	var owner = state ? state.owner : user.services_stamp_owner;
	if (!stamp || !owner || !ULine_Exists(owner)) return 0;
	if (peer.protocol == "bahamut") return peer.nick.toLowerCase() == owner ? stamp : 0;
	return stamp + "@" + owner;
}

function IRC_receive_nick_stamp(peer, user, value) {
	if (!peer.hub || !IRC_has_svcstate(peer)) return;
	var match = /^([0-9]+)@([^@\s]+)$/.exec(value), stamp;
	if (!match || !ULine_Exists(match[2])) return;
	stamp = IRC_parse_channel_timestamp(match[1]);
	if (stamp === null) return;
	/* Even an opaque hint can affect an external backend's split recovery.
	   A leaf cannot attest it. Trusted hub observations carry no flags here;
	   only a later services grant or hub snapshot can assert identity. */
	user.services_stamp = stamp; user.services_stamp_owner = match[2].toLowerCase();
}

function IRC_send_svcstate(peer, user) {
	var state = IRC_external_state(user);
	if (!state || !peer.admitted || !IRC_has_svcstate(peer)) return;
	var created = IRC_nick_timestamp_view(user, peer);
	if (created === null) return;
	var flags = (state.identified ? "r" : "") + (state.admin ? "a" : "");
	/* A snapshot is the direct sender's observation, not a forged prefix
	   locating the services authority on the wrong side of a returning link. */
	peer.ircout(format("SVCSTATE %s %lu %s %lu %s", user.nick, created,
		state.owner, state.stamp, flags || "-"));
}

function IRC_receive_svcstate(peer, origin, p) {
	/* Only a directly configured hub can restore observed grants. A leaf's
	   opaque NICK stamp is a reconnect hint, never identified/admin flags. */
	if (origin !== peer || !peer.hub || !IRC_has_svcstate(peer) || p.length != 5) return;
	var user = Users[p[0].toUpperCase()], created = IRC_parse_channel_timestamp(p[1]);
	var stamp = IRC_parse_channel_timestamp(p[3]), owner = p[2].toLowerCase();
	if (!user || created === null || created != user.created || stamp === null
		|| !ULine_Exists(owner) || !/^(?:r|a|ra|-)$/.test(p[4])) return;
	var next = {owner:owner, stamp:stamp, identified:p[4].indexOf("r") >= 0,
		admin:p[4].indexOf("a") >= 0, nick:user.nick.toUpperCase(), created:user.created};
	next.admin = !!(next.admin && next.identified && (user.mode & USERMODE_OPER));
	if (JSON.stringify(user.external_services) == JSON.stringify(next)) return;
	user.external_services = next; user.services_stamp = stamp;
	user.services_stamp_owner = owner;
	for (var name in Local_Servers) if (Local_Servers[name] !== peer) IRC_send_svcstate(Local_Servers[name], user);
}

function IRC_receive_svsmode(peer, origin, p) {
	if (!origin.uline || p.length < 2 || p[0][0] == "#" || p[0][0] == "&") return;
	var user = Users[p[0].toUpperCase()];
	if (!user) return;
	var index = 1, created = null;
	if (/^[0-9]+$/.test(p[index])) {
		created = IRC_parse_channel_timestamp(p[index++]);
		if (created === null || (created && created != IRC_nick_timestamp_view(user, peer))) return;
	}
	var modes = p[index++], add = true, updates = [], stamp;
	if (!modes || !/^[+\-rda]+$/.test(modes)) return;
	for (var i = 0; i < modes.length; i++) {
		var mode = modes[i];
		if (mode == "+" || mode == "-") { add = mode == "+"; continue; }
		if (mode == "d") {
			stamp = IRC_parse_channel_timestamp(p[index++]);
			if (stamp === null) return;
			updates.push({mode:mode, value:stamp});
		} else updates.push({mode:mode, value:add});
	}
	if (index != p.length || !updates.length) return;
	if (created && !user.local) {
		var home = Local_Servers[String(user.parent).toLowerCase()];
		if (!home || !home.admitted || IRC_nick_timestamp_view(user, home) === null) return;
	}
	var relays = [];
	for (var name in Local_Servers) {
		var next = Local_Servers[name];
		if (next === peer || !next.admitted || !IRC_has_svcstate(next)) continue;
		var fields = p.slice();
		if (created) {
			var target_time = IRC_nick_timestamp_view(user, next);
			/* Reject before changing any state, rather than weakening an
			   unavailable conditional address into an unconditional TS zero. */
			if (target_time === null) return;
			fields[1] = String(target_time);
		}
		relays.push({peer:next, fields:fields});
	}
	var owner = IRC_akill_owner(origin), state = IRC_external_state(user);
	if (!state || state.owner != owner)
		state = {owner:owner, stamp:user.services_stamp_owner == owner ? user.services_stamp || 0 : 0,
			identified:false, admin:false, nick:user.nick.toUpperCase(), created:user.created};
	updates.forEach(function(update) {
		if (update.mode == "d") state.stamp = update.value;
		else if (update.mode == "r") { state.identified = update.value; if (!update.value) state.admin = false; }
		else state.admin = update.value;
	});
	state.admin = !!(state.admin && state.identified && (user.mode & USERMODE_OPER));
	user.external_services = state; user.services_stamp = state.stamp;
	user.services_stamp_owner = owner;
	/* Keep the original authority on live grants, including matching hubs.
	   Old native peers ignore SVSMODE and cannot relay it reliably. */
	relays.forEach(function(relay) { relay.peer.originatorout("SVSMODE " + relay.fields.join(" "), origin); });
}

function IRC_services_send_tls(peer, user) {
	if (peer.protocol != "bahamut" || !peer.admitted || peer.quitting) return;
	if (user.parent && user.parent.toLowerCase() == peer.nick.toLowerCase()) return;
	peer.rawout(":" + user.servername + " MODE " + user.nick + " " + (IRC_client_tls_status(user) == "TLS" ? "+S" : "-S"));
}
