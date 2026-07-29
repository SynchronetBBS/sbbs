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

// --- games.local.ini, the sysop's overlay -----------------------------------
//
// The shipped catalog is replaced by an upgrade, so a game the sysop added has
// to live beside it. Merged by id, not substituted wholesale: their entries
// survive AND the games added upstream still arrive.
ok(gg_localName("/d/games.ini") === "/d/games.local.ini", "localName derives the sibling");
ok(gg_localName("/d/games") === "/d/games.local", "localName tolerates no extension");

var localfile = new File(dir + "games_test.local.ini");
if (!localfile.open("w"))
	throw new Error("cannot write the overlay fixture");
localfile.write("[game:zork1]\n"
	+ "name     = Zork I (sysop's copy)\n"
	+ "category = fantasy\n"
	+ "source   = bundled\n"
	+ "file     = zork1.z3\n"
	+ "\n"
	+ "[game:mygame]\n"
	+ "name     = A Game I Own\n"
	+ "category = mystery\n"
	+ "source   = bundled\n"
	+ "file     = mine.z5\n");
localfile.close();

var merged = gg_loadManifest(dir + "games_test.ini");
ok(merged.length === 3, "the overlay adds its game to the catalog  (got " + merged.length + ")");
ok(merged[0].id === "zork1" && merged[0].name === "Zork I (sysop's copy)",
	"an id in both files takes the sysop's entry");
ok(merged[0].name !== games[0].name, "...which really did differ from the shipped one");
ok(merged[1].id === games[1].id, "a shipped game the sysop did not touch is unchanged");
ok(merged[2].id === "mygame" && merged[2].category === "mystery",
	"a game only the sysop has is appended");

// The overlay alone is a working catalog: an install with no games.ini at all.
ok(gg_loadManifest(dir + "nosuch.ini") === null, "no file at all -> null");
var alone = gg_loadManifest(dir + "games_test.local.ini");
ok(alone !== null && alone.length === 2, "the overlay reads on its own too");

file_remove(dir + "games_test.local.ini");
ok(gg_loadManifest(dir + "games_test.ini").length === 2, "removing the overlay restores the catalog");

var ppm = js.exec_dir + "test.ppm";
ok(gg_sha1(ppm) === "358da797472140e5c533ba57826ee7c580f6964e", "sha1 of test.ppm matches sha1sum");
ok(gg_verify(ppm, "358da797472140e5c533ba57826ee7c580f6964e") === true, "verify match");
ok(gg_verify(ppm, "deadbeef") === false, "verify mismatch");
ok(gg_verify(ppm, "") === true, "verify with no expected hash -> ok");
ok(gg_sha1(js.exec_dir + "nope.xyz") === null, "sha1 of missing file -> null");

print(fail ? ("\n" + fail + " FAILURES") : "\nALL PASS");
exit(fail ? 1 : 0);
