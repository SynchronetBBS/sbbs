/* Hub-issued timestamps for channel objects on drifting leaves.
 * Cache one process incarnation per authenticated leaf name. Keep its live
 * object records across SQUIT; a reconnecting process reconciles the records
 * with BEGIN/CLAIM/END, and a replacement process retires the old incarnation.
 * Only configured hubs can replicate network timestamps. */
var Leaf_Timestamps = {};
var Leaf_Process = format("%08x%08x%08x%08x", random(2147483647),
	random(2147483647), random(2147483647), random(2147483647));
var Leaf_Channel_Sequence = 0;
var Leaf_Part_Sequence = 0;

function IRC_admit_waiting_leaves() {
	for (var i in Local_Servers)
		if (Local_Servers[i].awaiting_hub_maps)
			IRC_finish_clock_handshake(Local_Servers[i]);
}

function IRC_has_tsmap(peer) {
	return !peer.uline && peer.their_capab && peer.their_capab.indexOf("TSMAP") >= 0;
}

function IRC_is_leaf_uplink(peer) {
	return IRC_has_tsmap(peer) && peer.hub && peer.clock_role_received && !peer.clock_enforce;
}

function IRC_tsmap_broadcast(message, except) {
	for (var i in Local_Servers) {
		var peer = Local_Servers[i];
		if (peer !== except && peer.admitted && peer.hub && peer.clock_enforce && IRC_has_tsmap(peer))
			peer.ircout("TSMAP " + message);
	}
}

function IRC_tsmap_reset_line(name, record) {
	return "RESET " + name + " " + record.instance + " " + record.revision + " " + record.issuer;
}

function IRC_tsmap_set_line(name, record, entry) {
	return "SET " + name + " " + record.instance + " " + entry.id + " "
		+ entry.raw + " " + entry.net + " " + entry.name + " " + record.revision + " " + record.issuer;
}

function IRC_tsmap_snapshot(peer, name, record) {
	peer.ircout("TSMAP " + IRC_tsmap_reset_line(name, record));
	for (var id in record.objects)
		peer.ircout("TSMAP " + IRC_tsmap_set_line(name, record, record.objects[id]));
	peer.ircout("TSMAP END " + name + " " + record.instance + " " + record.revision + " " + record.issuer);
}

function IRC_tsmap_publish(name, record, except) {
	for (var i in Local_Servers) {
		var peer = Local_Servers[i];
		if (peer !== except && peer.admitted && peer.hub && peer.clock_enforce && IRC_has_tsmap(peer))
			IRC_tsmap_snapshot(peer, name, record);
	}
}

function IRC_tsmap_advance(record) {
	record.revision = Math.max(Epoch(), record.revision + 1);
	record.issuer = ServerName;
}

function IRC_tsmap_remove(name, record, id) {
	delete record.objects[id];
	IRC_tsmap_advance(record);
	IRC_tsmap_broadcast("DROP " + name + " " + record.instance + " " + id
		+ " " + record.revision + " " + record.issuer);
}

function IRC_tsmap_order(record) {
	return record.issuer + " " + record.instance;
}

function IRC_tsmap_burst(peer) {
	if (IRC_is_leaf_uplink(peer)) {
		peer.ircout("TSMAP BEGIN " + Leaf_Process);
		return;
	}
	if (!peer.hub || !peer.clock_enforce || !IRC_has_tsmap(peer)) return;
	for (var name in Leaf_Timestamps) {
		var record = Leaf_Timestamps[name];
		IRC_tsmap_snapshot(peer, name, record);
	}
}

function IRC_tsmap_claim(peer, chan) {
	if (IRC_is_leaf_uplink(peer))
		peer.ircout("TSMAP CLAIM " + Leaf_Process + " " + chan.leaf_timestamp_id
			+ " " + chan.leaf_timestamp + " " + chan.nam);
}

function IRC_tsmap_drop(chan) {
	if (chan.nam[0] != "#") return;
	for (var i in Local_Servers) {
		var peer = Local_Servers[i];
		if (peer.admitted && IRC_is_leaf_uplink(peer))
			peer.ircout("TSMAP DROP " + Leaf_Process + " " + chan.leaf_timestamp_id);
	}
}

function IRC_leaf_timestamp(peer, name) {
	var chan = Channels[name.toUpperCase()];
	if (!IRC_has_tsmap(peer)) return chan ? chan.created : Epoch();
	var claim = peer.timestamp_claim;
	delete peer.timestamp_claim;
	if (!claim || claim.name.toUpperCase() != name.toUpperCase() || !peer.timestamp_instance) {
		peer.quit("Missing channel timestamp identity"); return null;
	}
	var record = Leaf_Timestamps[peer.nick.toLowerCase()], entry = record.objects[claim.id];
	if (entry && (entry.name.toUpperCase() != name.toUpperCase() || entry.raw != claim.raw)) {
		peer.quit("Channel timestamp identity reused for another object"); return null;
	}
	if (!entry) {
		for (var previous in record.objects) {
			if (record.objects[previous].name.toUpperCase() != name.toUpperCase()) continue;
			if (chan && IRC_leaf_has_member(peer, chan)) {
				peer.quit("Active channel timestamp identity changed"); return null;
			}
			IRC_tsmap_remove(peer.nick, record, previous);
		}
		entry = record.objects[claim.id] = {id: claim.id, name: name, raw: claim.raw,
			net: chan ? chan.created : Epoch()};
		IRC_tsmap_advance(record);
		IRC_tsmap_broadcast(IRC_tsmap_set_line(peer.nick, record, entry));
	}
	if (peer.timestamp_seen) peer.timestamp_seen[claim.id] = true;
	/* The stored mapping supplies the age of a returning leaf-only channel.
	   It never replaces an already established hub channel incarnation. */
	return chan ? chan.created : entry.net;
}

function IRC_receive_tsmap(peer, origin, p) {
	var record, entry, name, id, instance, raw, net, revision, i;
	if (!IRC_has_tsmap(peer) || !origin.server) return;
	if (peer.hub && origin === peer && p[0] == "PARTACK" && p.length == 3
		&& /^[0-9]+[.][0-9]+$/.test(p[2])) {
		var user = Local_Users[p[2].split(".")[0]], key = p[1].toUpperCase();
		var pending = user && user.pending_channel_parts[key];
		if (pending && pending.peer === peer && pending.token == p[2])
			delete user.pending_channel_parts[key];
		return;
	}
	if (!peer.hub) {
		if (origin !== peer) return;
		name = peer.nick.toLowerCase(); record = Leaf_Timestamps[name];
		if (p[0] == "BEGIN" && p.length == 2 && /^[0-9a-f]{32}$/.test(p[1])) {
			if (peer.timestamp_instance) { peer.quit("Repeated timestamp process identity"); return; }
			peer.timestamp_instance = p[1]; peer.timestamp_seen = {};
			if (!record || record.instance != p[1]) {
				revision = Math.max(Epoch(), record ? record.revision + 1 : 0);
				record = Leaf_Timestamps[name] = {instance: p[1], revision: revision,
					issuer: ServerName, objects: {}};
			} else IRC_tsmap_advance(record);
			IRC_tsmap_publish(name, record);
			return;
		}
		if (!record || p[1] != peer.timestamp_instance || record.instance != p[1]) return;
		if (p[0] == "CLAIM" && p.length == 5 && /^[0-9]{1,20}$/.test(p[2])
			&& (raw = IRC_parse_channel_timestamp(p[3])) && p[4][0] == "#"
			&& p[4].length <= MAX_CHANLEN && !/[\x00-\x20,\xa0]/.test(p[4])) {
			/* One pending claim, consumed by the immediately following JOIN
			   or SJOIN. Claims alone cannot allocate an unbounded object map. */
			peer.timestamp_claim = {id: p[2], raw: raw, name: p[4]};
			return;
		}
		if (p[0] == "END" && peer.timestamp_seen) {
			for (id in record.objects)
				if (!peer.timestamp_seen[id]) {
					IRC_tsmap_remove(name, record, id);
				}
			delete peer.timestamp_seen;
			return;
		}
		if (p[0] == "DROP" && p.length == 3) {
			entry = record.objects[p[2]];
			/* A leaf cannot retire the identity of a channel it still uses. */
			if (entry) {
				var chan = Channels[entry.name.toUpperCase()];
				if (chan && IRC_leaf_has_member(peer, chan)) return;
				IRC_tsmap_remove(name, record, p[2]);
			}
		}
		return;
	}
	/* Hub messages carry the authenticated leaf name as well as its process
	   identity. These records are intentionally independent of Servers[] and
	   Channels[], both of which are destroyed during a netsplit. */
	name = p[1] && p[1].toLowerCase(); instance = p[2];
	if (!name || !/^[a-z0-9_.-]+$/.test(name) || !/^[0-9a-f]{32}$/.test(instance)) return;
	record = Leaf_Timestamps[name];
	if (p[0] == "RESET" && p.length == 5 && (revision = IRC_parse_channel_timestamp(p[3]))) {
		var active = Local_Servers[name];
		if (active && !active.hub && active.timestamp_instance
			&& active.timestamp_instance != instance) return;
		peer.timestamp_snapshot = {name: name, instance: instance, revision: revision, issuer: p[4], objects: {}};
		return;
	}
	var snapshot = peer.timestamp_snapshot;
	if (snapshot && snapshot.name == name && snapshot.instance == instance) {
		if (p[0] == "SET" && p.length == 9 && Number(p[7]) == snapshot.revision
			&& p[8] == snapshot.issuer && /^[0-9]{1,20}$/.test(p[3])
			&& (raw = IRC_parse_channel_timestamp(p[4])) && (net = IRC_parse_channel_timestamp(p[5]))
			&& p[6][0] == "#" && p[6].length <= MAX_CHANLEN && !/[\x00-\x20,\xa0]/.test(p[6])) {
			snapshot.objects[p[3]] = {id: p[3], raw: raw, net: net, name: p[6]};
			return;
		}
		if (p[0] != "END" || p.length != 5 || Number(p[3]) != snapshot.revision || p[4] != snapshot.issuer) return;
		delete peer.timestamp_snapshot;
		var wins = !record || snapshot.revision > record.revision
			|| (snapshot.revision == record.revision && IRC_tsmap_order(snapshot) >= IRC_tsmap_order(record));
		var winner = wins ? snapshot : record, loser = wins ? record : snapshot, changed = false;
		/* The newest inventory retires ended objects, so an old hub burst
		   cannot resurrect them. Common surviving objects keep the earliest
		   hub-issued mapping if a partition produced concurrent sightings. */
		if (loser && loser.instance == winner.instance) {
			for (id in winner.objects) {
				entry = winner.objects[id]; var old = loser.objects[id];
				if (old && old.raw == entry.raw && old.name.toUpperCase() == entry.name.toUpperCase() && old.net < entry.net) {
					entry.net = old.net; changed = true;
				}
			}
		}
		if (changed) IRC_tsmap_advance(winner);
		Leaf_Timestamps[name] = winner;
		if (wins || changed) IRC_tsmap_publish(name, winner, peer);
		return;
	}
	if (!record || record.instance != instance) return;
	var update_revision = IRC_parse_channel_timestamp(p[p.length - 2]), update_issuer = p[p.length - 1];
	if (!update_revision || update_revision < record.revision
		|| (update_revision == record.revision && update_issuer < record.issuer)) return;
	if (p[0] == "SET" && p.length == 9 && /^[0-9]{1,20}$/.test(p[3])
		&& (raw = IRC_parse_channel_timestamp(p[4])) && (net = IRC_parse_channel_timestamp(p[5]))
		&& p[6][0] == "#" && p[6].length <= MAX_CHANLEN && !/[\x00-\x20,\xa0]/.test(p[6])) {
		entry = record.objects[p[3]];
		if (entry && (entry.raw != raw || entry.name.toUpperCase() != p[6].toUpperCase())) return;
		/* Concurrent first sightings after a partition converge on one age. */
		if (entry) net = Math.min(net, entry.net);
		entry = record.objects[p[3]] = {id: p[3], name: p[6], raw: raw, net: net};
		record.revision = update_revision; record.issuer = update_issuer;
		IRC_tsmap_broadcast(IRC_tsmap_set_line(name, record, entry), peer);
	} else if (p[0] == "DROP" && p.length == 6) {
		delete record.objects[p[3]];
		record.revision = update_revision; record.issuer = update_issuer;
		IRC_tsmap_broadcast(p.join(" "), peer);
	}
}
