// jsexec exec/load/tests/test_game_lobby_events.js
var gl = load({}, "game_lobby.js");
var jl = load({}, "json_lines.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=" + got + " want=" + want + (ok ? " ok" : (fails++, " FAIL")));
}
var path = system.temp_dir + "gl_ev_test.jsonl";
var f = new File(path); f.open("w+"); f.close();      // fresh
jl.add(path, { time: 100, type: "level", map: "E1L1" });
jl.add(path, { time: 200, type: "death", map: "E1L1" });
f = new File(path); f.open("a"); f.write("this is not json\n"); f.close();   // bad line
jl.add(path, { time: 300, type: "level", map: "E1L2" });

var ev = gl.read_events(path, 10);
chk("read count (recover skips bad line)", ev.length, 3);
chk("read newest last", ev[ev.length - 1].map, "E1L2");

var feed = gl.event_feed(path, 10, function (e) {
	return e.type === "level" ? ("level " + e.map) : null;   // skip death
});
chk("feed skips non-level", feed.length, 2);
chk("feed newest first", feed[0], "level E1L2");

var kept = gl.prune_events(path, 2, 2);                // 3 lines > cap 2 -> keep 2
chk("prune kept", kept, 2);
chk("prune file now 2", gl.read_events(path, 10).length, 2);

chk("mmss", gl.mmss(125), "2:05");

// ago(): bracket format, shared with the who's-online view (matches SyncDOOM).
chk("ago now", gl.ago(time()), "[now]");
chk("ago minutes", gl.ago(time() - 125), "[2m]");
chk("ago hours", gl.ago(time() - 7200), "[2h]");

// Missing file tolerated: read -> [], prune -> 0 (no throw).
var missing = system.temp_dir + "gl_ev_missing.jsonl";
if (file_exists(missing)) file_remove(missing);
chk("read missing -> []", gl.read_events(missing, 10).length, 0);
chk("prune missing -> 0", gl.prune_events(missing, 2, 2), 0);

writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
