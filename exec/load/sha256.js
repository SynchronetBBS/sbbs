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
// It is a POLYFILL in the load/*.js sense (cf. array.js's Array.prototype
// shims): loading it installs a File.sha256_hex GETTER shaped EXACTLY like the
// native md5_hex / sha1_hex, so a script written against `file.sha256_hex`
// today keeps working unchanged the day a build ships the property natively --
// at which point this installs nothing and the C getter is used instead. The
// guard is `"sha256_hex" in File.prototype`: the native hash getters live on
// File.prototype (js_file.cpp's property table), so its arrival there is what
// silently retires the shim.
//
//     load("sha256.js");
//     var f = new File(path);
//     if (f.open("rb")) { var hex = f.sha256_hex; f.close(); }
//
//     // or, for the open-hash-close-by-path common case:
//     var hex = sha256_of_file(path);
//
// Matches the native getters' semantics precisely, because a polyfill that
// behaved differently would be a trap once the real one lands:
//   - the file must be OPEN; on a closed File the getter is `undefined`, as
//     md5_hex is;
//   - the WHOLE file is hashed regardless of the current read position, and the
//     position is restored afterward (reading the digest does not disturb an
//     in-progress read);
//   - the result is a lowercase hex string.
//
// sha256_of_file(path) is a convenience wrapper -- open "rb", read the getter,
// close -- returning the digest or null if the file cannot be opened.
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

// Install File.sha256_hex only when the build lacks it -- see the header. A
// native property on File.prototype always wins this test and this branch is
// skipped, so the two never collide.
if (!("sha256_hex" in File.prototype)) {
	Object.defineProperty(File.prototype, "sha256_hex", {
		configurable: true,       // a shim, not a fixture: replaceable/deletable
		enumerable: false,        // native getters are non-enumerable; match them
		get: function () {
			var cc, chunk, hex, pos;

			// Closed file -> undefined, exactly as md5_hex/sha1_hex report.
			if (!this.is_open)
				return undefined;

			// Hash the whole file from the top, then put the read position
			// back: the native getter leaves an in-progress read undisturbed,
			// so this one must too.
			pos           = this.position;
			this.position = 0;
			try {
				// CryptContext.ALGO.SHA2 IS SHA-256 -- a fixed 256-bit digest,
				// not a family selector (src/sbbs3/js_cryptcon.cpp). Reading
				// .hashvalue is what finalizes the context: its getter calls
				// cryptEncrypt(ctx, p, 0) before fetching the attribute, so no
				// explicit final call is needed.
				//
				// read() in "rb" mode yields one character per byte, so raw
				// bytes -- high bit set, embedded NULs, 0x1A -- round-trip
				// through encrypt() intact. 64 KB at a time keeps a large image
				// out of memory; the chunk size does not affect the digest.
				cc = new CryptContext(CryptContext.ALGO.SHA2);
				while ((chunk = this.read(65536)) != null && chunk.length > 0)
					cc.encrypt(chunk);
				hex = sha256_hexify(cc.hashvalue);
			} finally {
				this.position = pos;
			}
			return hex;
		}
	});
}

function sha256_of_file(path)
{
	var f = new File(path);
	var hex;

	if (!f.open("rb"))
		return null;
	try {
		hex = f.sha256_hex;
	} finally {
		f.close();
	}
	// A native getter yields a lowercase hex string already; the shim does too.
	// Normalize regardless so a future native casing change cannot surprise a
	// caller comparing against a lowercase constant.
	return hex === undefined ? null : String(hex).toLowerCase();
}
