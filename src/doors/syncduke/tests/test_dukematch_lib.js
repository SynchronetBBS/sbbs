// jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_dukematch_lib.js
//
// Headless unit tests for the SyncDuke dukematch model layer (syncduke_lib.js).
// Loads the checkout lib as a namespace object (it ends with `this;`) and stubs
// `user` (jsexec has none). The lib's internal bare load of game_lobby.js resolves
// to the live install copy -- fine, this task does not touch game_lobby.js.
user = { number: 1, alias: "Duke" };

var lib = load({}, js.exec_dir + "../../../../xtrn/syncduke/syncduke_lib.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}
function has(n, hay, needle) {
	var ok = String(hay).indexOf(needle) >= 0;
	writeln("  " + n + (ok ? " ok" : (fails++, " FAIL -- [" + needle + "] not in [" + hay + "]")));
}
function hasnot(n, hay, needle) {
	var ok = String(hay).indexOf(needle) < 0;
	writeln("  " + n + (ok ? " ok" : (fails++, " FAIL -- [" + needle + "] unexpectedly in [" + hay + "]")));
}

// --- sd_cmd: mode -> /c flag ---
var coop = lib.sd_cmd("master", 25000, null, 1, "coop");
has("coop -> /c2", coop, " /c2");   hasnot("coop no /c1", coop, " /c1");
var dm = lib.sd_cmd("master", 25000, null, 1, "dm", { monsters: false, respawn_items: false });
has("dm -> /c1", dm, " /c1");       hasnot("dm no /c2", dm, " /c2");
var dmns = lib.sd_cmd("master", 25000, null, 1, "dmnospawn", { monsters: false, respawn_items: false });
has("dmnospawn -> /c3", dmns, " /c3");
var deflt = lib.sd_cmd("master", 25000, null, 1);           // omitted mode -> coop (back-compat)
has("default mode -> /c2", deflt, " /c2");

// --- sd_cmd: dukematch options (/m monsters-off default, /t item respawn) ---
has("dm monsters-off default -> /m", dm, " /m");
var dmMon = lib.sd_cmd("master", 25000, null, 1, "dm", { monsters: true, respawn_items: false });
hasnot("dm monsters on -> no /m", dmMon, " /m");
var dmResp = lib.sd_cmd("master", 25000, null, 1, "dm", { monsters: false, respawn_items: true });
has("dm respawn_items -> /t", dmResp, " /t");
hasnot("dm no respawn_items -> no /t", dm, " /t");
hasnot("coop never adds /m", coop, " /m");

// --- sd_load_config: [dukematch] defaults present ---
var cfg = lib.sd_load_config();
chk("cfg.dukematch.submode default", cfg.dukematch.submode, "spawn");
chk("cfg.dukematch.monsters default", cfg.dukematch.monsters, "false");

// --- registry mode round-trip (real fs, cleaned up) ---
mkpath(system.data_dir + "syncduke/games/");
var f1 = lib.sd_write_entry(cfg, 29001, 1, "dm");
var f2 = lib.sd_write_entry(cfg, 29002, 1);                // no mode -> coop
var open = lib.sd_list_open_games(cfg);
function byPort(list, p) { for (var i = 0; i < list.length; i++) if (String(list[i].port) === String(p)) return list[i]; return null; }
var e1 = byPort(open, 29001), e2 = byPort(open, 29002);
chk("entry dm mode", e1 && e1.mode, "dm");
chk("entry default mode coop", e2 && e2.mode, "coop");
if (f1) file_remove(f1);
if (f2) file_remove(f2);

// --- sd_event_text: frag case (SyncDOOM-verbatim) ---
chk("frag text", lib.sd_event_text({ type: "frag", killer: "Duke", victim: "Rob" }), "Duke fragged Rob");
chk("death text unchanged", lib.sd_event_text({ type: "death", user: "Duke", map: "E1L1" }), "Duke died on E1L1");
chk("unknown type -> null", lib.sd_event_text({ type: "bogus" }), "null");

writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
