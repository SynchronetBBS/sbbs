// jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_waiting_room_lib.js
//
// Headless tests for the SyncDuke waiting-room registry model (syncduke_lib.js):
// the heartbeat field + the .claim coordination helpers. Loads the lib as a
// namespace object and stubs `user` (jsexec has none).
user = { number: 1, alias: "Duke" };

var lib = load({}, js.exec_dir + "../../../../xtrn/syncduke/syncduke_lib.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}

mkpath(system.data_dir + "syncduke/games/");
var cfg = lib.sd_load_config();

// --- heartbeat field is written and fresh ---
var path = lib.sd_write_entry(cfg, 29050, 1, "coop");
chk("entry written", (path && file_exists(path)) ? "yes" : "no", "yes");
var f = new File(path); f.open("r"); var g = f.iniGetObject(); f.close();
var hb = parseInt(g.heartbeat, 10);
chk("heartbeat present + recent", (hb > 0 && (time() - hb) < 5) ? "ok" : ("bad:" + g.heartbeat), "ok");

// --- claim path transform ---
chk("claim path transform", lib.sd_claim_path(path), path.replace(/\.ini$/, ".claim"));

// --- claim round-trip: not claimed -> claim -> claimed; second claim rejected ---
chk("not claimed initially", lib.sd_game_claimed(path), "false");
chk("claim write ok", lib.sd_claim_game({ file: path }), "true");
chk("claimed after claim", lib.sd_game_claimed(path), "true");
chk("second claim rejected (exclusive)", lib.sd_claim_game({ file: path }), "false");

// --- clear removes BOTH entry and claim, releasing the exclusive lock ---
lib.sd_clear_game(path);
chk("entry removed", file_exists(path), "false");
chk("claim removed", file_exists(lib.sd_claim_path(path)), "false");

// --- heartbeat re-write advances the timestamp on the same file ---
var p2 = lib.sd_write_entry(cfg, 29051, 1, "dm");
var f2 = new File(p2); f2.open("r"); var g2a = f2.iniGetObject(); f2.close();
lib.sd_write_entry(cfg, 29051, 1, "dm");                 // re-heartbeat (same stem/path)
var f3 = new File(p2); f3.open("r"); var g2b = f3.iniGetObject(); f3.close();
chk("re-write keeps same path", p2, lib.sd_write_entry(cfg, 29051, 1, "dm"));
chk("heartbeat monotonic", (parseInt(g2b.heartbeat,10) >= parseInt(g2a.heartbeat,10)) ? "ok" : "back", "ok");
lib.sd_clear_game(p2);

writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
