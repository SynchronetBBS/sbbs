/* Preserve 6bd095d0's queued state/QUIT flush across all shutdown paths. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function shutdown_state() {
	var s = new_state();
	s.warnings = [];
	s.log = function(level, message) { if (level == LOG_WARNING) s.warnings.push(message); };
	s.exit = function(code) { s.exited = true; };
	s.js.setInterval = function() { return 1; };
	s.Die_Password = s.Restart_Password = "";
	s.user = new_user(s, "Local");
	s.user.socket = {send:function(line) { return line.length; }, close:function() {}};
	new_user(s, "Remote", "peer.test");
	return s;
}

function shutdown_peer(s, name, chunk) {
	var peer = new_peer(s, name || "peer.test", true);
	peer.socket = {is_connected:true, wire:"", calls:0, closed:false,
		send:function(data) {
			this.calls++;
			var count = chunk === undefined ? data.length : Math.min(chunk, data.length);
			this.wire += data.substr(0, count);
			return count;
		},
		close:function() { this.closed = true; this.is_connected = false; }};
	peer.sendq = new s.IRC_Queue(peer);
	peer.rawout = function(line) { this.sendq.add(line); };
	return peer;
}

run_tests({
	"shutdown flushes queued channel state before local QUITs without running callbacks": function() {
		var s = shutdown_state(), peer = shutdown_peer(s, "peer.test", 7);
		peer.rawout(":Local PART #old");
		peer.rawout(":Local MODE #service -o Local");
		s.Notify_Servers_Of_Shutdown("Shutdown test");
		var wire = peer.socket.wire;
		check(wire.indexOf(":Local PART #old\r\n") < wire.indexOf(":Local MODE #service -o Local\r\n"));
		check(wire.indexOf(":Local MODE #service -o Local\r\n") < wire.indexOf(":Local QUIT :Shutdown test\r\n"));
		check(wire.indexOf(":Remote QUIT") < 0 && peer.sendq.empty, "Shutdown duplicated remote departures or left output queued");
		var calls = peer.socket.calls;
		s.Notify_Servers_Of_Shutdown("Exit hook");
		check(peer.socket.calls == calls && peer.socket.wire == wire, "Shutdown notification was not idempotent");
	},
	"embedded service QUIT/SQUIT messages are included in the final synchronous flush": function() {
		var s = shutdown_state(), peer = shutdown_peer(s);
		var callbacks = s.js;
		s.js = js;
		try { load(s, "ircd/services-common.js"); load(s, "ircd/services.js"); }
		finally { s.js = callbacks; }
		s.system.qwk_id = "TEST"; s.system.data_dir = IRCD_TEST_TEMP_DIR; s.DNS_Resolver = {};
		s.EmbeddedServices_Config.enabled = true;
		var manager = s.IRC_Services = new s.IRC_Services_Manager(s.EmbeddedServices_Config);
		manager.localhost = true; manager.snapshot = function() {};
		manager.introduce();
		var chan = join_user(s, s.user, "#service");
		manager.chanserv.do_join("#service");
		peer.sendq.purge(); peer.socket.wire = "";
		s.Notify_Servers_Of_Shutdown("Shutdown test");
		var wire = peer.socket.wire;
		check(manager.stopped && !manager.central && !s.Users.NICKSERV && !s.Users.CHANSERV);
		check(wire.indexOf(":NickServ QUIT :Services stopped\r\n") >= 0 && wire.indexOf(":ChanServ QUIT :Services stopped\r\n") >= 0, "Embedded departures were queued after the final flush");
		check(wire.indexOf("SQUIT services.synchro.net") > wire.indexOf(":ChanServ QUIT"), "Service server quit before its users");
		check(wire.indexOf(":Local QUIT :Shutdown test\r\n") >= 0 && peer.sendq.empty);
		s.Notify_Servers_Of_Shutdown("Exit hook");
		check(peer.socket.wire == wire && peer.sendq.empty);
		/* Apply the actual departure stream to another server. */
		var remote = new_state(), input = new_peer(remote, s.ServerName, true);
		var service = new remote.IRC_Server();
		service.nick = "services.synchro.net"; service.parent = input.nick; service.linkparent = input.nick; service.local = false;
		remote.Servers[service.nick] = service;
		var observer = new_user(remote, "Observer"), local = new_user(remote, "Local", input.nick);
		var rc = join_user(remote, observer, "#service"); join_user(remote, local, "#service");
		["NickServ", "ChanServ"].forEach(function(nick) {
			var user = new_user(remote, nick, input.nick); user.servername = service.nick;
			if (nick == "ChanServ") join_user(remote, user, "#service");
		});
		wire.split("\r\n").forEach(function(line) { if (line) input.work(line); });
		check(!remote.Users.LOCAL && !remote.Users.NICKSERV && !remote.Users.CHANSERV && !remote.Servers[service.nick], "Peer retained users or services after shutdown");
		check(rc.users[observer.id] === observer && Object.keys(rc.users).length == 1, "Peer retained ghost memberships");
	},
	"a blocked or broken link does not prevent other links receiving shutdown QUITs": function() {
		var s = shutdown_state(), blocked = shutdown_peer(s, "blocked.test", 0), broken = shutdown_peer(s, "broken.test"), healthy = shutdown_peer(s, "healthy.test");
		broken.sendq.send = function() { throw new Error("Broken link"); };
		s.Notify_Servers_Of_Shutdown("Shutdown test");
		check(blocked.socket.calls > 0 && blocked.socket.calls <= 100, "A blocked shutdown drain was not bounded");
		check(healthy.socket.wire.indexOf(":Local QUIT :Shutdown test\r\n") >= 0 && healthy.sendq.empty);
		check(s.warnings.some(function(message) { return message.indexOf("flush error") >= 0; }));
	},
	"services teardown failure does not prevent local QUIT delivery": function() {
		var s = shutdown_state(), peer = shutdown_peer(s);
		s.IRC_Services = {stop:function() { throw new Error("Backend stop failed"); }};
		s.Notify_Servers_Of_Shutdown("Shutdown test");
		check(peer.socket.wire.indexOf(":Local QUIT :Shutdown test\r\n") >= 0 && peer.sendq.empty);
		check(s.warnings.some(function(message) { return message.indexOf("services stop error") >= 0; }));
	},
	"the exit hook closes every client, link and listening socket": function() {
		var s = shutdown_state(), peer = shutdown_peer(s), closed = [];
		function listener(name) { return {close:function() { closed.push(name); }}; }
		s.user.socket.close = function() { closed.push("user"); };
		s.Unregistered.x = {socket:{close:function() { throw new Error("Already closed"); }}};
		s.PLine_Sockets.push(listener("pline"));
		s.server = {socket:listener("server")};
		s.Close_All_Sockets();
		check(peer.socket.closed && closed.join() == "user,pline,server", "Sockets left open: " + closed.join());
		check(s.PLine_Sockets.length == 0 && s.warnings.length == 1);
	},
	"DIE flushes departures before stopping callbacks and the exit backstop does not repeat them": function() {
		var s = shutdown_state(), peer = shutdown_peer(s);
		s.user.mode |= s.USERMODE_OPER; s.user.flags |= s.OLINE_CAN_DIE;
		s.user.work("DIE");
		check(s.js.do_callbacks === false && peer.socket.wire.indexOf(":Local QUIT :Server shut down by Local\r\n") >= 0);
		var wire = peer.socket.wire;
		s.Notify_Servers_Of_Shutdown("Exit hook");
		check(peer.socket.wire == wire && peer.sendq.empty);
	},
	"RESTART flushes departures before closing sockets and exits only after delivery": function() {
		var s = shutdown_state(), peer = shutdown_peer(s);
		s.user.mode |= s.USERMODE_OPER; s.user.flags |= s.OLINE_CAN_RESTART;
		s.user.work("RESTART");
		check(s.exited && s.js.do_callbacks === false && peer.socket.closed);
		var wire = peer.socket.wire;
		check(wire.indexOf(":Local QUIT :Aieeeee!!!  Restarting server...\r\n") >= 0);
		check(wire.indexOf(" QUIT ") < wire.indexOf("ERROR :"), "Sockets were closed before departures were flushed");
		s.Notify_Servers_Of_Shutdown("Exit hook");
		check(peer.socket.wire == wire);
	}
});
