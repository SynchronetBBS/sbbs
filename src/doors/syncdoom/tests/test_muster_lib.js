// jsexec /home/rswindell/sbbs/src/doors/syncdoom/tests/test_muster_lib.js
user = { number: 1, alias: "Host" };
var lib = load({}, js.exec_dir + "../../../../xtrn/syncdoom/syncdoom_lib.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}
mkpath(system.data_dir + "syncdoom/games/");
var cfg = lib.sd_load_config();
var ws = { id: "shareware", name: "DOOM Shareware", maxplayers: 4 };

var entry = lib.sd_write_muster(cfg, 26000, ws, "deathmatch", 4, 1);
chk("muster entry written", (entry && file_exists(entry)) ? "yes" : "no", "yes");
var f = new File(entry); f.open("r"); var g = f.iniGetObject(); f.close();
chk("status mustering", g.status, "mustering");
chk("mode carried", g.mode, "deathmatch");
chk("target carried", g.target, "4");
chk("players >=1 (host)", (parseInt(g.players, 10) >= 1) ? "ok" : "bad", "ok");
chk("heartbeat present", (parseInt(g.heartbeat, 10) > 0) ? "ok" : "bad", "ok");

// assembled count = waiters + 1
chk("players() with 0 waiters", lib.sd_muster_players(entry), 1);
var gl2 = load({}, "game_lobby.js");
gl2.write_waiter(entry, 7, "Bob");
chk("players() with 1 waiter", lib.sd_muster_players(entry), 2);

// sd_list_games surfaces the mustering entry (players>=1)
var games = lib.sd_list_games(cfg);
function byPort(list, p) { for (var i = 0; i < list.length; i++) if (String(list[i].port) === String(p)) return list[i]; return null; }
var e = byPort(games, 26000);
chk("mustering listed", e ? e.status : "(missing)", "mustering");

gl2.clear_muster(entry);
writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
