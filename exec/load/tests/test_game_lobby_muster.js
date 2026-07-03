// jsexec exec/load/tests/test_game_lobby_muster.js
var gl = load({}, "game_lobby.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}
var dir = backslash(system.temp_dir + "gl_muster_test");
mkpath(dir);
var entry = gl.write_game(dir, "host-25000", { status: "mustering", port: 25000, players: 1 });
chk("entry written", (entry && file_exists(entry)) ? "yes" : "no", "yes");

// waiter markers
chk("no waiters initially", gl.list_waiters(entry).length, 0);
chk("write waiter ok", gl.write_waiter(entry, 3, "Bob"), "true");
chk("same node rejected (exclusive)", gl.write_waiter(entry, 3, "Bob2"), "false");
gl.write_waiter(entry, 5, "Alice");
var w = gl.list_waiters(entry);
chk("two waiters", w.length, 2);
function aliasOf(list, node) { for (var i = 0; i < list.length; i++) if (String(list[i].node) === String(node)) return list[i].alias; return null; }
chk("waiter 3 alias", aliasOf(w, 3), "Bob");
chk("waiter 5 alias", aliasOf(w, 5), "Alice");
gl.remove_waiter(entry, 3);
chk("after remove one", gl.list_waiters(entry).length, 1);

// go marker
chk("no go initially", gl.read_go(entry), "null");
chk("write go ok", gl.write_go(entry, "127.0.0.1:25000"), "true");
chk("read go", gl.read_go(entry), "127.0.0.1:25000");

// clear removes everything
gl.clear_muster(entry);
chk("clear removes go", gl.read_go(entry), "null");
chk("clear removes waiters", gl.list_waiters(entry).length, 0);
chk("clear removes entry", file_exists(entry), "false");

writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
