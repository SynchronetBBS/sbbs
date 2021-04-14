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
			if (this.sendline !== undefined) { // client went away between queueing and running this callback? Alls I knows is that this.sendline may be undefined here.
				this.sendline(sdata);
			}
		}).bind(clients[c], sdata)); // bind because otherwise 'sdata is undefined'
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
					user: u,
					session: s,
				});
			} else {
				var update = false;
				if (current[u].xtrn !== sessions.local.users[u].xtrn) {
					sessions.local.users[u].xtrn = current[u].xtrn;
					update = true;
				}
				if (current[u].sessions[s].action !== sessions.local.users[u].sessions[s].action) {
					sessions.local.users[u].sessions[s].action = current[u].sessions[s].action;
					update = true;
				}
				if (update) {
					broadcast('update', {
						user: sessions.local.users[u],
						session: s,
					});
				}
				delete current[u].sessions[s];
			}
		}
		if (Object.keys(sessions.local.users[u].sessions).length < 1) delete sessions.local.users[u];
		if (current[u] !== undefined && current[u].sessions !== undefined && Object.keys(current[u].sessions).length < 1) delete current[u];
	}

	for (var u in current) {
		if (sessions.local.users[u] === undefined) sessions.local.users[u] = current[u];
		for (var s in current[u].sessions) {
			sessions.local.users[u].sessions[s] = current[u].sessions[s];
			broadcast('logon', {
				user: sessions.local.users[u],
				session: s,
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
		broadcast('system', {
			host: sys.host,
			name: sys.name
		});
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
				host: sys.host,
				user: sessions.remote[sys.host].users[e.name],
				session: skey,
			});
		} else if (sessions.remote[sys.host].users[e.name].sessions[skey].action !== e.action) {
			sessions.remote[sys.host].users[e.name].sessions[skey].action = e.action;
			broadcast('update', {
				system: sys.name,
				host: sys.host,
				user: sessions.remote[sys.host].users[e.name],
				session: skey,
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
		host: sys.host,
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

function onClient() {
	const sock = this.accept();
	if (!sock) {
		log(LOG_DEBUG, '!Error accepting connection: ' + sock.error);
		return;
	}
	if (idle()) refresh(false);
	clients[sock.descriptor] = sock;
	sock.once('read', function () {
		if (this.is_connected) this.close();
		delete clients[this.descriptor];
		if (idle()) refresh(true);
	});
	sock.once('write', function () {
		this.sendline(JSON.stringify({ event: 'state', data: sessions }));
	});
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
	js.setInterval(imsg.read_sys_list.bind(imsg), settings.remote_list_refresh); // bind because otherwise 'this.sys_list is undefined'
	js.setImmediate(imsg.read_sys_list.bind(imsg)); // bind because otherwise 'this.sys_list is undefined'
}

refresh(true); // Start in 'idle' mode, set refresh timers, do an initial poll

js.do_callbacks = true;
