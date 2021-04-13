load('sbbsdefs.js');
load('nodedefs.js');
const imsg = load({}, 'sbbsimsg_lib.js');

/* Override these defaults by adding corresponding keys to [presence_service]
 * in modopts.ini. All 'refresh' intervals are in milliseconds. Set 'local' or
 * 'remote' to false if you want to disable local or interBBS presence.
 */
const defaultSettings = {
	local: true,
	remote: true, // Must set up instant messaging first: http://wiki.synchro.net/module:sbbsimsg
	local_refresh: 5000, 
	remote_refresh: 30000,
	idle_local_refresh: 20000,
	idle_remote_refresh: 120000,
	remote_list_refresh: 3600000,
};

const settings = load('modopts.js', 'presence_service') || {};
for (var s in defaultSettings) {
	if (settings[s] === undefined) settings[s] = defaultSettings[s];
}

const clients = {};

const sessions = {
	local: { users: {} },
	remote: {},
};

const intervals = {
	localRefresh: null,
	remoteRefresh: null,
};

function idle() {
	return (Object.keys(clients).length < 1);
}

function broadcast(evt, data) {
	if (idle()) return;
	const sdata = JSON.stringify({ event: evt, data: data });
	for (var c in clients) {
		clients[c].once('write', (function (sdata) {
			/* It's possible that onClientData has nuked this socket already.
			 * Check that 'this' still has 'sendline' to avoid an exception.
			 */
			if (this.sendline !== undefined) this.sendline(sdata);
		}).bind(clients[c], sdata)); // Using bind "temporarily" pending better closure+callback stuff
	}
}

function getLocalSessions() {
	
	const users = {};
	
	const u = new User(1);
	system.node_list.forEach(function (e, i) {
		if (e.status !== NODE_INUSE) return;
		u.number = e.useron;
		if (!users[u.alias]) {
			users[u.alias] = {
				name: u.alias,
				gender: u.gender,
				age: u.age,
				location: u.location,
				xtrn: xtrn_area.prog[u.curxtrn].name,
				sessions: {},
			};
		}
		users[u.alias].sessions[i] = { action: NodeAction[e.action] };
	});

	directory(system.data_dir + 'user/*.web').forEach(function (e) {
		if (time() - file_date(e) >= 900) return;
		const f = new File(e);
		if (!f.open('r')) return;
		const session = f.iniGetObject();
		f.close();
		const base = file_getname(e).replace(file_getext(e), '');
		const un = parseInt(base, 10);
		if (isNaN(un) || un < 1 || un > system.lastuser) return;
		u.number = un;
		if (!users[u.alias]) {
			users[u.alias] = {
				name: u.alias,
				gender: u.gender,
				age: u.age,
				location: u.location,
				xtrn: xtrn_area.prog[u.curxtrn].name,
				sessions: {},
			};
		}
		users[u.alias].sessions.W = { action: session.action };
	});

	return users;

}

function scanLocal() {

	if (!settings.local) return;

	const current = getLocalSessions();

	for (var u in sessions.local.users) {
		for (var s in sessions.local.users[u].sessions) {
			if (current[u] === undefined || current[u].sessions[s] === undefined) {
				delete sessions.local.users[u].sessions[s];
				broadcast('logoff', {
					system: 'local',
					user: u,
					session: s,
				});
			} else {
				const updates = {};
				if (current[u].xtrn !== sessions.local.users[u].xtrn) {
					sessions.local.users[u].xtrn = current[u].xtrn;
					updates.xtrn = sessions.local.users[u].xtrn;
				}
				if (current[u].sessions[s].action !== sessions.local.users[u].sessions[s].action) {
					sessions.local.users[u].sessions[s].action = current[u].sessions[s].action;
					updates.sessions = {};
					updates.sessions[s] = { action: sessions.local.users[u].sessions[s].action };
				}
				if (Object.keys(updates).length > 0) {
					broadcast('update', {
						system: 'local',
						user: u,
						data: updates,
					});
				}
				delete current[u].sessions[s];
			}
		}
		if (Object.keys(sessions.local.users[u].sessions).length < 1) delete sessions.local.users[u];
		if (Object.keys(current[u].sessions).length < 1) delete current[u];
	}

	for (var u in current) {
		if (sessions.local.users[u] === undefined) sessions.local.users[u] = current[u];
		for (var s in current[u].sessions) {
			sessions.local.users[u].sessions[s] = current[u].sessions[s];
			broadcast('logon', {
				system: 'local',
				user: u,
				session: s,
				data: sessions.local.users[u],
			});
		}
	}

}

function getRemoteSessions() {
	if (settings.remote) imsg.request_active_users();
}

function onRemoteLogon(usr, sys) {

	if (!settings.remote) return;

	if (sys === null) return; // Only the first result contains system details

	if (!sessions.remote[sys.host]) {
		sessions.remote[sys.host] = {
			host: sys.host,
			name: sys.name,
			users: {}
		};
	}

	sys.users.forEach(function (e) {
		if (!sessions.remote[sys.host].users[e.name]) {
			sessions.remote[sys.host].users[e.name] = {
				name: e.name,
				gender: e.sex,
				age: e.age,
				location: e.location,
				xtrn: e.xtrn,
				sessions: {},
			};
		}
		const skey = e.prot === 'web' ? 'W' : e.node;
		if (!sessions.remote[sys.host].users[e.name].sessions[skey]) {
			sessions.remote[sys.host].users[e.name].sessions[skey] = { action: e.action };
			broadcast('logon', {
				system: sys.name,
				user: e.name,
				session: skey,
				data: sessions.remote[sys.host].users[e.name],
			});
		} else if (sessions.remote[sys.host].users[e.name].sessions[skey].action !== e.action) {
			sessions.remote[sys.host].users[e.name].sessions[skey].action = e.action;
			broadcast('update', {
				system: sys.name,
				user: e.name,
				session: skey,
				data: sessions.remote[sys.host].users[e.name],
			});
		}
	});

}

function onRemoteLogoff(usr, sys) {
	if (!settings.remote) return;
	if (!sessions.remote[sys.host]) return;
	if (!sessions.remote[sys.host].users[usr.name]) return;
	const skey = usr.prot === 'web' ? 'W' : usr.node;
	if (!sessions.remote[sys.host].users[usr.name].sessions[skey]) return;
	delete sessions.remote[sys.host].users[usr.name].sessions[skey];
	if (!Object.keys(sessions.remote[sys.host].users[usr.name].sessions).length) delete sessions.remote[sys.host].users[usr.name];
	broadcast('logoff', {
		system: sys.name,
		user: usr.name,
		session: skey,
	});
}

function refresh(idling) {
	if (settings.local) {
		if (intervals.localRefresh !== null) js.clearInterval(intervals.localRefresh);
		js.setInterval(scanLocal, idling ? settings.idle_local_refresh : settings.local_refresh);
		js.setImmediate(scanLocal);
	}
	if (settings.remote) {
		if (intervals.remoteRefresh !== null) js.clearInterval(intervals.remoteRefresh);
		js.setInterval(getRemoteSessions, idling ? settings.idle_remote_refresh : settings.remote_refresh);
		js.setImmediate(getRemoteSessions);
	}
}

function onRemoteData() {
	const msg = imsg.receive_active_users();
	imsg.parse_active_users(msg, onRemoteLogon, onRemoteLogoff);
}

function onClientData() {
	if (this.is_connected) return;
	delete clients[this.descriptor];
	// This would be the place to nuke any on/once listeners still in place for 'this'
	if (idle()) refresh(true); // We are now idling, so lengthen the refresh intervals
}

function onClient() {
	const sock = server.socket.accept();
	if (!sock) {
		log(LOG_DEBUG, '!Error accepting connection: ' + sock.error);
		return;
	}
	if (idle()) refresh(false); // We are no longer idling, so shorten the refresh intervals
	clients[sock.descriptor] = sock;
	sock.on('read', onClientData);
	sock.once('write', (function (sessions) {
		sock.sendline(JSON.stringify(sessions));
	}).bind(sock, sessions)); // Using bind "temporarily" pending better closure+callback stuff
}

if (this.server === undefined) { // jsexec

	var port = 10011;
	var addr = [ '0.0.0.0', '::' ];

	for (arg = 0; arg < argc; arg++) {
		switch (argv[arg].toLowerCase()) {
			case "-p":
				port = parseInt(argv[++arg]);
				break;
			case "-a":
				addr = argv[++arg].split(',');
				break;
			default:
				break;
		}
	}

	const server = { socket: new ListeningSocket(addr, port, "Presence") };
	if (!server.socket) exit();

}

server.socket.on('read', onClient);
imsg.sock.on('read', onRemoteData);

if (settings.remote) {
	/* This service may run indefinitely, so we should reload the BBS list once in
	 * a while so we'll pick up new systems to poll and drop any old ones.
	 */
	js.setInterval(imsg.read_sys_list.bind(imsg), settings.remote_list_refresh);
	js.setImmediate(imsg.read_sys_list.bind(imsg));
}

refresh(true); // Start in 'idle' mode, set refresh timers, do an initial poll

js.auto_terminate = true;
js.do_callbacks = true;
