/* Implicit TLS for server links. GPL-2.0-or-later. */
"use strict";
var IRC_TLS_CA_Cache = null;

function IRC_tls_clear_cache() {
	if (IRC_TLS_CA_Cache) IRC_TLS_CA_Cache.certs.forEach(function(cert) { cert.destroy(); });
	IRC_TLS_CA_Cache = null;
}

function IRC_tls_port(port) {
	return Number(port) == 6697 || Number(port) == 994 || !!TLS_Ports[Number(port)];
}
function IRC_tls_required(cline, port) {
	return !!cline.tls || Number(port) == 6697 || Number(port) == 994;
}

function IRC_tls_optional_attribute(cert, attribute) {
	try { return cert.get_attribute(attribute); }
	catch (error) {
		/* CryptCert exposes cryptlib's NOTFOUND status only in its error text. */
		if (!/Cryptlib error -43\b/.test(String(error))) throw error;
		return undefined;
	}
}

function IRC_tls_check_certificate(cert) {
	var now = Date.now(), from = Number(cert.validfrom), to = Number(cert.validto);
	if (!isFinite(from) || !isFinite(to) || now < from || now > to)
		throw new Error("TLS peer certificate is expired or not yet valid");
	/* Use the attribute API: some runtimes return undefined for the extended
	   usage properties even when those extensions are present. */
	if (IRC_tls_optional_attribute(cert, CryptCert.ATTR.EXTKEYUSAGE) !== undefined
		&& !IRC_tls_optional_attribute(cert, CryptCert.ATTR.EXTKEY_SERVERAUTH)
		&& !IRC_tls_optional_attribute(cert, CryptCert.ATTR.EXTKEY_ANYKEYUSAGE))
		throw new Error("TLS peer certificate is not valid for server authentication");
}

function IRC_tls_check_chain_usage(peer) {
	if (peer.certtype != CryptCert.TYPE.CERTCHAIN) {
		IRC_tls_check_certificate(peer);
		return;
	}
	peer.current_certificate = CryptCert.CURSOR.FIRST;
	try {
		for (var count = 0; count < 100; count++) {
			IRC_tls_check_certificate(peer);
			try { peer.current_certificate = CryptCert.CURSOR.NEXT; }
			catch (error) {
				if (/Cryptlib error -43\b/.test(String(error))) return;
				throw error;
			}
		}
		throw new Error("TLS peer certificate chain is too long");
	} finally { peer.current_certificate = CryptCert.CURSOR.FIRST; }
}

function IRC_tls_ca_certificates() {
	if (!TLS_Config.ca_file) throw new Error("Configure [TLS] CAFile before enabling TLS server links");
	var path = TLS_Config.ca_file;
	if (!/^(?:\/|[a-z]:[\\/]|\\\\)/i.test(path)) path = system.ctrl_dir + path;
	var stamp = file_date(path), cache = IRC_TLS_CA_Cache;
	if (cache && cache.path == path && cache.stamp == stamp) return cache.certs;
	var file = new File(path), certs = [];
	if (!file.open("rb")) throw new Error("Cannot open TLS CA bundle " + path);
	var text;
	try { text = file.read(); } finally { file.close(); }
	var pattern = /-----BEGIN CERTIFICATE-----([\s\S]*?)-----END CERTIFICATE-----/g, match;
	try {
		while ((match = pattern.exec(text)) !== null) {
			var cert = new CryptCert(base64_decode(match[1].replace(/\s/g, "")));
			certs.push(cert);
			if (!cert.ca) throw new Error("TLS CA bundle contains a certificate that is not a CA");
		}
		if (!certs.length) throw new Error("TLS CA bundle contains no certificates");
	} catch (error) { certs.forEach(function(c) { c.destroy(); }); throw error; }
	if (cache) cache.certs.forEach(function(c) { c.destroy(); });
	IRC_TLS_CA_Cache = {path:path, stamp:stamp, certs:certs};
	return certs;
}

function IRC_tls_connect(socket) {
	if (typeof socket.tls_nameverify != "boolean" || typeof socket.tls_certverify != "boolean" || typeof CryptCert != "function")
		throw new Error("This Synchronet build cannot validate TLS server links");
	/* Read the trust configuration before starting TLS. Never send an IRC
	   password until both the handshake and explicit chain check succeed. */
	var roots = IRC_tls_ca_certificates();
	socket.tls_minver = 102;
	socket.tls_nameverify = true;
	socket.tls_certverify = true;
	socket.tls_enhanced_certcheck = true;
	socket.ssl_session = true;
	if (!socket.ssl_session || !socket.tls_remote_cert) throw new Error("TLS peer did not supply a certificate");
	var peer = socket.tls_remote_cert;
	IRC_tls_check_chain_usage(peer);
	/* Some cryptlib versions expose a certverify flag without checking a
	   trust chain. CryptCert.check() does that check explicitly. Trust is
	   installed only for this synchronous check, preserving prior settings. */
	var changed = [];
	try {
		roots.forEach(function(root) {
			if (!root.trusted_implicit) { root.trusted_implicit = true; changed.push(root); }
		});
		if (peer.selfsigned && !peer.trusted_implicit) throw new Error("TLS peer certificate is self-signed and untrusted");
		if (!peer.check()) throw new Error("TLS peer certificate chain is untrusted");
	} finally { changed.reverse().forEach(function(root) { root.trusted_implicit = false; }); }
	socket.irc_tls_verified = true;
}
