// getgames.js -- provision tested Z-machine story files for the door.
//
// Reads games.ini and, per [game:*]: verifies bundled games, and (prompted, one
// game at a time) downloads the rest, baking v6 graphics (Arthur) via
// tools/blorb2gfx.js. Idempotent (a re-run only does what's missing) and
// non-fatal (any failure is reported and skipped). Run by the installer
// (install-xtrn.ini [exec:getgames.js]) or by hand:
//     jsexec ../xtrn/zmachine/getgames.js
// SpiderMonkey 1.8.5-compatible. Interactive-only.

load("http.js");

function gg_dir() {
	var d = js.exec_dir || js.startup_dir || "./";
	if (d.charAt(d.length - 1) != "/" && d.charAt(d.length - 1) != "\\")
		d += "/";
	return d;
}

// "<dir>/games.ini" -> "<dir>/games.local.ini".
function gg_localName(path) {
	var dot = String(path).lastIndexOf(".");

	if (dot < 0)
		return path + ".local";
	return path.substr(0, dot) + ".local" + path.substr(dot);
}

function gg_readManifest(path) {
	var f = new File(path);

	if (!f.open("r"))
		return null;
	var list = f.iniGetAllObjects("id", "game:");
	f.close();
	return list;
}

// The catalog, with the sysop's own entries layered over it.
//
// games.ini ships with the door and is replaced by an upgrade, so a sysop who
// added a game they own would lose it. games.local.ini beside it is theirs, and
// is merged by game id rather than replacing the catalog: their entries survive,
// and the games added upstream still arrive. An id in both files takes the
// sysop's version -- that is how a shipped entry gets corrected locally.
//
// Returns null only when NEITHER file can be read.
function gg_loadManifest(path) {
	var base  = gg_readManifest(path);
	var local = gg_readManifest(gg_localName(path));
	var out   = [];
	var seen  = {};
	var i, id;

	if (!base && !local)
		return null;
	base  = base || [];
	local = local || [];

	for (i = 0; i < local.length; i++)
		seen[String(local[i].id).toLowerCase()] = local[i];

	// Shipped order first, so the catalog reads the same as it always has, with
	// any locally-corrected entry substituted in place.
	for (i = 0; i < base.length; i++) {
		id = String(base[i].id).toLowerCase();
		if (seen[id] !== undefined) {
			out.push(seen[id]);
			seen[id] = undefined;
		} else {
			out.push(base[i]);
		}
	}
	// Then whatever the sysop added that the catalog has never heard of.
	for (i = 0; i < local.length; i++) {
		id = String(local[i].id).toLowerCase();
		if (seen[id] !== undefined)
			out.push(local[i]);
	}
	return out;
}

function gg_targetPath(dir, game) {
	return dir + game.category + "/" + game.file;
}

function gg_decide(game, present) {
	if (String(game.source).toLowerCase() == "bundled")
		return present ? "verify" : "missing";
	return present ? "skip" : "fetch";
}

function gg_needsBake(game, gfxPresent) {
	return String(game.bake).toLowerCase() == "true" && !gfxPresent;
}

function gg_sha1(path) {
	var f = new File(path);
	if (!f.open("rb"))
		return null;
	var h = f.sha1_hex;
	f.close();
	return h ? String(h).toLowerCase() : null;
}

// true = ok (hashes match, OR no expected hash supplied); false = mismatch.
function gg_verify(path, expected) {
	if (!expected)
		return true;
	var got = gg_sha1(path);
	return got !== null && got == String(expected).toLowerCase();
}

function gg_haveConvert() {
	return system.exec("convert -version") == 0;   // ImageMagick
}

function gg_download(url, dest) {
	var ok = false;
	try {
		var req = new HTTPRequest();
		var n = req.Download(url, dest);
		if (req.response_code && req.response_code >= 400)
			print("  ! HTTP " + req.response_code);
		else
			ok = n > 0 && file_exists(dest) && file_size(dest) > 0;
	} catch (e) {
		print("  ! download error: " + e);
	}
	// Never leave a partial/empty/error-body file behind: HTTPRequest.Download writes the
	// destination even on a 404, and a stray file would make a later run treat the game as
	// already installed (breaking idempotency and masking the failed fetch).
	if (!ok && file_exists(dest))
		file_remove(dest);
	return ok;
}

// Fetch the Blorb if needed, then bake <story>.gfx via tools/blorb2gfx.js.
function gg_bake(dir, game) {
	var story  = gg_targetPath(dir, game);
	var gfxDir = story.replace(/\.[zZ]\d$/, "") + ".gfx";
	var blorb  = dir + game.category + "/" + game.blorb;
	if (!file_exists(blorb)) {
		if (!game.blorb_url) { print("  ! no Blorb source for " + game.name); return false; }
		print("  fetching graphics: " + game.blorb_url + " ...");
		if (!gg_download(game.blorb_url, blorb)) { print("  ! Blorb download failed"); return false; }
	}
	if (!gg_haveConvert()) {
		print("  ! ImageMagick 'convert' not found -- v6 graphics for " + game.name
			+ " will fall back to text. Install ImageMagick and re-run getgames.js.");
		return false;
	}
	print("  baking " + game.blorb + " -> " + gfxDir + " ...");
	return js.exec(dir + "tools/blorb2gfx.js", dir, {}, blorb, gfxDir) == 0;
}

// True only if this install's exec/load/http.js provides HTTPRequest.Download() (added relatively
// recently). On older Synchronet, downloads can't work -- detect it so we say so clearly instead of
// throwing an opaque "Download is not a function" deep in gg_download().
function gg_downloadSupported() {
	try { return typeof (new HTTPRequest()).Download === "function"; }
	catch (e) { return false; }
}

function gg_main() {
	var dir = gg_dir();
	var games = gg_loadManifest(dir + "games.ini");
	if (!games) { print("getgames: cannot read " + dir + "games.ini"); return 1; }

	var canDL = gg_downloadSupported();
	if (!canDL)
		print("getgames: NOTE -- this Synchronet's exec/load/http.js lacks HTTPRequest.Download(),\r\n"
			+ "  so game downloads are unavailable. Bundled games still install; to fetch the rest,\r\n"
			+ "  update Synchronet (or drop in a current exec/load/http.js) and re-run.\r\n");

	var i, g, path, action, gfx;
	for (i = 0; i < games.length; i++) {
		g = games[i];
		path = gg_targetPath(dir, g);
		action = gg_decide(g, file_exists(path));

		if (action == "verify") {
			if (gg_verify(path, g.sha1)) print(g.name + ": present");
			else print(g.name + ": WARNING -- present but SHA-1 mismatch (" + path + ")");
		} else if (action == "missing") {
			print(g.name + ": MISSING bundled file " + path);
		} else if (action == "skip") {
			print(g.name + ": already installed");
		} else { // fetch
			if (!canDL) { print(g.name + ": not installed (download unavailable -- see note above)"); continue; }
			if (g.note) print("\r\n" + g.name + " -- " + g.note);
			if (!confirm("Fetch " + g.name + " now")) { print(g.name + ": skipped"); continue; }
			if (!file_isdir(dir + g.category)) mkdir(dir + g.category);
			print("  downloading " + g.url + " ...");
			if (!gg_download(g.url, path)) { print("  ! download failed -- skipping " + g.name); continue; }
			if (!gg_verify(path, g.sha1))
				print("  note: " + g.name + " differs from the tested SHA-1 -- may be a newer/older revision; verify it plays.");
			print("  installed " + path);
		}

		// Resource Blorb (<story>.blb): fetched for ANY game that names one, not
		// just bake=true -- sampled sounds (The Lurking Horror, Sherlock) are read
		// from the .blb at play time, no pre-bake step. Runs on re-runs too, so a
		// sysop who installed a game before its Blorb was listed just re-runs this.
		if (file_exists(path) && g.blorb && g.blorb_url && canDL
		    && !file_exists(dir + g.category + "/" + g.blorb)) {
			print("  fetching resources: " + g.blorb_url + " ...");
			if (gg_download(g.blorb_url, dir + g.category + "/" + g.blorb))
				print("  installed " + g.blorb);
			else
				print("  ! Blorb download failed -- " + g.name + " plays without its media");
		}

		gfx = path.replace(/\.[zZ]\d$/, "") + ".gfx";
		if (file_exists(path) && gg_needsBake(g, file_isdir(gfx))) {
			if (gg_bake(dir, g)) print("  graphics baked");
		}
	}
	print("\r\ngetgames: done.");
	return 0;
}

if (typeof GETGAMES_NO_MAIN == "undefined")
	gg_main();
