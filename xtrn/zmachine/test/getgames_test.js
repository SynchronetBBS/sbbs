GETGAMES_NO_MAIN = 1;
load(js.exec_dir + "../getgames.js");

var dir = js.exec_dir + "fixtures/";
var fail = 0;
function ok(cond, msg) { if (!cond) { fail++; print("FAIL: " + msg); } else print("ok: " + msg); }

var games = gg_loadManifest(dir + "games_test.ini");
ok(games && games.length === 2, "loadManifest returns 2 games");
ok(games[0].id === "zork1" && games[0].category === "fantasy", "first game parsed");
ok(gg_targetPath("/d/", games[0]) === "/d/fantasy/zork1.z3", "targetPath");
ok(gg_decide(games[0], true) === "verify", "bundled present -> verify");
ok(gg_decide(games[0], false) === "missing", "bundled absent -> missing");
ok(gg_decide(games[1], true) === "skip", "download present -> skip");
ok(gg_decide(games[1], false) === "fetch", "download absent -> fetch");
ok(gg_needsBake(games[1], false) === true, "bake=true & no gfx -> bake");
ok(gg_needsBake(games[1], true) === false, "bake=true & gfx present -> no bake");
ok(gg_needsBake(games[0], false) === false, "no bake key -> no bake");

var ppm = js.exec_dir + "test.ppm";
ok(gg_sha1(ppm) === "358da797472140e5c533ba57826ee7c580f6964e", "sha1 of test.ppm matches sha1sum");
ok(gg_verify(ppm, "358da797472140e5c533ba57826ee7c580f6964e") === true, "verify match");
ok(gg_verify(ppm, "deadbeef") === false, "verify mismatch");
ok(gg_verify(ppm, "") === true, "verify with no expected hash -> ok");
ok(gg_sha1(js.exec_dir + "nope.xyz") === null, "sha1 of missing file -> null");

print(fail ? ("\n" + fail + " FAILURES") : "\nALL PASS");
exit(fail ? 1 : 0);
