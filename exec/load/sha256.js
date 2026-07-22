// sha256.js -- SHA-256 of a file's contents, streamed.
//
// Synchronet hashes files natively through File.md5_hex / File.md5_base64
// (3.11) and File.sha1_hex / File.sha1_base64 (3.19). Both stream the file
// rather than loading it, which is what makes them usable on a 600 MB disc
// image. There is no SHA-256 equivalent at either level -- not on File, and not
// among the md5_calc()/sha1_calc() globals, which take a string and so would
// need the whole file in memory anyway.
//
// This fills that gap with cryptlib's CryptContext, which every build already
// links in for TLS. Ten xtrn/ fetchers had grown their own copy of this
// function; it lives here once instead.
//
//     load("sha256.js");
//     var hex = sha256_of_file(path);
//
// Written as a POLYFILL: should a future build add File.sha256_hex, that is
// used directly and the CryptContext path simply falls away -- no call site
// changes, and older installs keep working unchanged.
//
// Returns a lowercase hex digest, or null if the file cannot be opened.
// Deliberately null rather than an exception: callers compare the result
// against a pinned constant, where null simply fails the comparison, and
// throwing would escape callbacks documented not to throw (the verify callback
// in exec/load/xtrn_mirror.js).
//
// SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Copyright (C) 2026 Rob Swindell.  GPL-2.0.

// Hex-encode a raw byte string (e.g. CryptContext.hashvalue). Prefixed rather
// than named hexify() because everything here shares one global namespace.
function sha256_hexify(s)
{
	var out = "", i, c;

	for (i = 0; i < s.length; i++) {
		c = s.charCodeAt(i) & 0xff;
		out += (c < 16 ? "0" : "") + c.toString(16);
	}
	return out;
}

function sha256_of_file(path)
{
	var f = new File(path);
	var cc, chunk, hex;

	if (!f.open("rb"))
		return null;
	try {
		// Native fast path if this build has it (see the polyfill note above).
		if (f.sha256_hex !== undefined)
			return String(f.sha256_hex).toLowerCase();

		// CryptContext.ALGO.SHA2 IS SHA-256 -- a fixed 256-bit digest, not a
		// family selector (src/sbbs3/js_cryptcon.cpp). Reading .hashvalue is
		// what finalizes the context: its getter calls cryptEncrypt(ctx, p, 0)
		// before fetching the attribute, so no explicit final call is needed.
		//
		// File.read() in "rb" mode yields one character per byte, so the raw
		// bytes -- high bit set, embedded NULs, 0x1A -- round-trip through
		// encrypt() intact. 64 KB at a time keeps a large image out of memory;
		// the chunk size does not affect the digest.
		cc = new CryptContext(CryptContext.ALGO.SHA2);
		while ((chunk = f.read(65536)) != null && chunk.length > 0)
			cc.encrypt(chunk);
		hex = sha256_hexify(cc.hashvalue);
	} finally {
		f.close();
	}
	return hex;
}
