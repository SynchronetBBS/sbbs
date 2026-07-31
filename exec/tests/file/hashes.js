// Digest properties of the File class (md5_hex/md5_base64, sha1_hex/
// sha1_base64, sha256_hex/sha256_base64) and their whole-string global
// counterparts (md5_calc, sha1_calc, sha256_calc).
//
// Expected values are the published test vectors: RFC 1321 for MD5, FIPS
// 180-2 for SHA-1 and SHA-256.
//
// All three File digests share one digest buffer and one read loop in
// js_file.cpp, so each algorithm is checked in both encodings -- a change
// made for one of them can silently truncate or over-read another.

var VECTORS = [
	{
		name: "empty",
		data: "",
		md5_hex:    "d41d8cd98f00b204e9800998ecf8427e",
		md5_b64:    "1B2M2Y8AsgTpgAmY7PhCfg==",
		sha1_hex:   "da39a3ee5e6b4b0d3255bfef95601890afd80709",
		sha1_b64:   "2jmj7l5rSw0yVb/vlWAYkK/YBwk=",
		sha256_hex: "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
		sha256_b64: "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU="
	}, {
		name: "abc",
		data: "abc",
		md5_hex:    "900150983cd24fb0d6963f7d28e17f72",
		md5_b64:    "kAFQmDzST7DWlj99KOF/cg==",
		sha1_hex:   "a9993e364706816aba3e25717850c26c9cd0d89d",
		sha1_b64:   "qZk+NkcGgWq6PiVxeFDCbJzQ2J0=",
		sha256_hex: "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
		sha256_b64: "ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0="
	}, {
		// 56 bytes: one byte too long to pad into a single 64-byte block,
		// so this is the two-block case for all three algorithms.
		name: "fips-56",
		data: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		md5_hex:    "8215ef0796a20bcaaae116d3876c664a",
		md5_b64:    "ghXvB5aiC8qq4RbTh2xmSg==",
		sha1_hex:   "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
		sha1_b64:   "hJg+RBw70m66rkqh+VEp5eVGcPE=",
		sha256_hex: "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
		sha256_b64: "JI1qYdIGOLjlwCaTDD5gOaM85Flk/yFn9uzt1BnbBsE="
	}
];

var path = system.temp_dir + "test_file_hashes_" + Date.now() + ".bin";

function check(what, got, want)
{
	if (got !== want)
		throw new Error(what + " = '" + got + "' instead of '" + want + "'");
}

// Write in binary mode so no platform's newline translation can alter the
// bytes the digest is taken over.
function write_file(data)
{
	var f = new File(path);

	if (!f.open("wb"))
		throw new Error("could not create " + path + ": " + f.error);
	if (data.length && !f.write(data))
		throw new Error("could not write " + path + ": " + f.error);
	f.close();
}

try {
	VECTORS.forEach(function (v) {
		check("md5_calc('" + v.name + "', true)", md5_calc(v.data, true), v.md5_hex);
		check("md5_calc('" + v.name + "')", md5_calc(v.data), v.md5_b64);
		check("sha1_calc('" + v.name + "', true)", sha1_calc(v.data, true), v.sha1_hex);
		check("sha1_calc('" + v.name + "')", sha1_calc(v.data), v.sha1_b64);
		check("sha256_calc('" + v.name + "', true)", sha256_calc(v.data, true), v.sha256_hex);
		check("sha256_calc('" + v.name + "')", sha256_calc(v.data), v.sha256_b64);

		write_file(v.data);
		var f = new File(path);
		if (!f.open("rb"))
			throw new Error("could not open " + path + ": " + f.error);
		try {
			check("File(" + v.name + ").md5_hex", f.md5_hex, v.md5_hex);
			check("File(" + v.name + ").md5_base64", f.md5_base64, v.md5_b64);
			check("File(" + v.name + ").sha1_hex", f.sha1_hex, v.sha1_hex);
			check("File(" + v.name + ").sha1_base64", f.sha1_base64, v.sha1_b64);
			check("File(" + v.name + ").sha256_hex", f.sha256_hex, v.sha256_hex);
			check("File(" + v.name + ").sha256_base64", f.sha256_base64, v.sha256_b64);
		} finally {
			f.close();
		}
	});

	// Larger than the 4 KB block the getters read in, and spanning every byte
	// value, so a chunk boundary or a sign-extended byte would show up here.
	var big = "";
	for (var i = 0; i < 10000; i++)
		big += String.fromCharCode(i & 0xff);
	write_file(big);

	var f = new File(path);
	if (!f.open("rb"))
		throw new Error("could not open " + path + ": " + f.error);
	try {
		check("File(10000 bytes).md5_hex", f.md5_hex, md5_calc(big, true));
		check("File(10000 bytes).sha1_hex", f.sha1_hex, sha1_calc(big, true));
		check("File(10000 bytes).sha256_hex", f.sha256_hex, sha256_calc(big, true));

		// Reading a digest must not disturb an in-progress read.
		f.position = 1234;
		f.sha256_hex;
		f.md5_hex;
		f.sha1_hex;
		if (f.position !== 1234)
			throw new Error("position = " + f.position + " after digest, expected 1234");
	} finally {
		f.close();
	}

	// On a closed File every digest property reports undefined.
	["md5_hex", "md5_base64", "sha1_hex", "sha1_base64",
	 "sha256_hex", "sha256_base64"].forEach(function (prop) {
		if (f[prop] !== undefined)
			throw new Error("closed File." + prop + " = '" + f[prop] + "', expected undefined");
	});
} finally {
	file_remove(path);
}
