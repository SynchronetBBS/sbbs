// xtrn_mirror.js -- fall back to Synchronet's asset mirror when a door's
// upstream publisher cannot be reached.
//
// Every xtrn/<door>/ that needs third-party game data downloads it from the
// publisher at install time -- a dozen different hosts nobody here controls.
// When one goes away, renames a release, or refuses the request, the door
// simply cannot be installed and the sysop has no recourse but to find the
// file by hand. Vertrauen keeps a copy of every such asset, so this turns a
// dead upstream into a second try rather than a dead end.
//
//     load("xtrn_mirror.js");
//     var n = xtrn_mirror_download(url, tmp, {
//             name:   "ra95-cd1.iso",
//             verify: function(p) { return sha256_of_file(p) == EXPECT; }
//     });
//     if (!n) { ... both sources failed ... }
//
// `name` defaults to the URL's basename, URL-decoded, which is right for every
// asset whose mirrored copy kept its published filename. Pass it only where the
// mirror name differs: a fetcher that renames on the way in, or the libretro
// buildbot, which serves every platform's build as the same <core>.zip and so
// needs a platform-qualified name to coexist in one flat directory.
//
// `verify` is what makes a CHECKSUM FAILURE fall back too, and not merely an
// unreachable host. A mismatch is usually a truncated transfer, and re-pulling
// from a different host is the right answer. It matters for a second reason:
// without a check, the mirror would be trusted exactly as blindly as the
// upstream, so adding a source would weaken every fetcher rather than
// strengthen it.
//
// Never throws. Every fetcher treats a failed download as non-fatal -- the
// sysop can always supply the file by hand -- and that stays true. Returns the
// number of bytes downloaded, or 0 if BOTH sources failed.
//
//     -mirror-only   skip the official source (exercises the fallback)
//     -no-mirror     disable the fallback (exercises the official source alone)
//
// SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Copyright (C) 2026 Rob Swindell.  GPL-2.0.

load("http.js");

var XTRN_MIRROR_BASE = "http://web.synchro.net/files/main/games/";

// Basename of a URL: strip #fragment, then ?query, then everything through the
// last '/', then percent-decode.
function xtrn_mirror_name(url)
{
	var s = String(url);
	var i;

	if ((i = s.indexOf("#")) >= 0)
		s = s.substring(0, i);
	if ((i = s.indexOf("?")) >= 0)
		s = s.substring(0, i);
	if ((i = s.lastIndexOf("/")) >= 0)
		s = s.substring(i + 1);
	return decodeURIComponent(s);
}

// True if `opt` appears in this script's command line.
function xtrn_mirror_flag(opt)
{
	var i;

	if (typeof argv == "undefined" || typeof argc == "undefined")
		return false;
	for (i = 0; i < argc; i++)
		if (String(argv[i]) == opt)
			return true;
	return false;
}

// One attempt at one URL. Returns bytes on success, 0 on any failure, and
// removes the partial file so the next attempt starts from clean ground.
function xtrn_mirror_try(url, path, verify)
{
	var req = new HTTPRequest();
	var n = 0;

	req.follow_redirects = 5;
	try {
		n = req.Download(url, path);
	} catch (e) {
		print("  ! " + url + ": " + e);
		n = 0;
	}
	// Download() returns 0 for any non-2xx response (http.js) -- but it has
	// already streamed the error body into `path`, so the cleanup below is
	// what stops a 404 page from masquerading as the archive.
	if (!n) {
		if (req.response_code)
			print("  ! " + url + ": HTTP " + req.response_code);
	} else if (typeof verify == "function" && !verify(path)) {
		print("  ! " + url + ": failed verification");
		n = 0;
	}
	if (!n && file_exists(path))
		file_remove(path);
	return n;
}

// Download `url` to `path`, falling back to the Synchronet mirror.
function xtrn_mirror_download(url, path, opts)
{
	var n;
	var tried = false;

	if (!opts)
		opts = {};

	if (!xtrn_mirror_flag("-mirror-only")) {
		tried = true;
		n = xtrn_mirror_try(url, path, opts.verify);
		if (n)
			return n;
	}
	if (xtrn_mirror_flag("-no-mirror"))
		return 0;

	// Deliberately does not name a cause: the reason (unreachable, HTTP
	// status, or bad bytes) was just printed above, and this same line is
	// reached when the source answered fine but failed verification.
	print(tried
	    ? "  ! falling back to the Synchronet mirror ..."
	    : "  -mirror-only: fetching from the Synchronet mirror ...");

	return xtrn_mirror_try(XTRN_MIRROR_BASE
	    + (opts.name ? opts.name : xtrn_mirror_name(url)), path, opts.verify);
}
