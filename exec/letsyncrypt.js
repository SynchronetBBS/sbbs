require("acmev2.js", "ACMEv2");

/*
 * File names used...
 */
var ks_fname = backslash(system.ctrl_dir)+"letsyncrypt.key";
var setting_fname = backslash(system.ctrl_dir)+"letsyncrypt.ini";
var sks_fname = backslash(system.ctrl_dir)+"ssl.cert";
var sbbsini_fname = backslash(system.ctrl_dir)+"sbbs.ini";
var maincnf_fname = backslash(system.ctrl_dir)+"main.cnf";
var recycle_sem = backslash(system.ctrl_dir)+"recycle.web";

/*
 * Get the Web Root
 */
var sbbsini = new File(sbbsini_fname);
if (!sbbsini.open("r"))
	throw("Unable to open "+sbbsini.name);
var webroot = backslash(sbbsini.iniGetValue("Web", "RootDirectory", "../web/root"));
sbbsini.close();

/*
 * Now read in the system password which must be used to encrypt the 
 * private key.
 * 
 * TODO: What happens when the system password changes?
 */
var maincnf = new File(maincnf_fname);
if (!maincnf.open("rb", true))
	throw("Unable to open "+maincnf.name);
maincnf.position = 186; // Indeed.
var syspass = maincnf.read(40);
syspass = syspass.replace(/\x00/g,'');
maincnf.close();

if (!older_than(30))
	exit(0);

/*
 * Now open/create the keyset and RSA signing key for
 * ACME.  Note that this key is not the one used for the
 * final certificate.
 */
var opts = CryptKeyset.KEYOPT.NONE;
if (!file_exists(ks_fname))
	opts = CryptKeyset.KEYOPT.CREATE;
var ks = new CryptKeyset(ks_fname, opts);

/*
 * The ACME key uses "ACMEv2" as the label.
 * 
 * TODO: Regenerate keys etc.
 */
var rsa;
try {
	rsa = ks.get_private_key("ACMEv2", syspass);
}
catch(e) {
	rsa = new CryptContext(CryptContext.ALGO.RSA);
	rsa.keysize=2048/8;
	rsa.label="ACMEv2";
	rsa.generate_key();
	ks.add_private_key(rsa, syspass);
}

/*
 * We store the key ID in our ini file so we don't need an extra
 * round-trip each session to discover it.
 */
var settings = new File(setting_fname);
settings.open(settings.exists ? "r+" : "w+");
var key_id = settings.iniGetValue("key_id", system.inet_addr, undefined);
var acme = new ACMEv2({key:rsa, key_id:key_id});
if (acme.key_id === undefined) {
	acme.create_new_account({termsOfServiceAgreed:true,contact:["mailto:sysop@"+system.inet_addr]});
}
/*
 * After the ACMEv2 object is created, we will always have a key_id
 * Write it to our ini if it wasn't there already.
 */
if (key_id === undefined) {
	settings.iniSetValue("key_id", system.inet_addr, acme.key_id);
	key_id = acme.key_id;
}
settings.close();

/*
 * Create the order, using system.inet_addr
 * 
 * TODO: SNAs... or something.
 */
var order = acme.create_new_order({identifiers:[{type:"dns",value:system.inet_addr}]});
var authz;
var challenge;
var auth;
var fulfilled;
var token;
var completed=0;

/*
 * Find an http-01 authorization
 */
for (auth in order.authorizations) {
	fulfilled = false;
	authz = acme.get_authorization(order.authorizations[auth]);
	for (challenge in authz.challenges) {
		if (authz.challenges[challenge].type=='http-01') {
			/*
			 * Create a place to store the challenge and store it there
			 * 
			 * TODO: Clean up stale files
			 */
			mkpath(webroot+".well-known/acme-challenge");
			token = new File(backslash(webroot+".well-known/acme-challenge")+authz.challenges[challenge].token);
			token.open("w");
			token.write(authz.challenges[challenge].token+"."+acme.thumbprint());
			token.close();
			acme.accept_challenge(authz.challenges[challenge]);
			fulfilled = true;
		}
	}
	/*
	 * TODO: We can run all these in parallel rather than in series...
	 */
	if (fulfilled) {
		while (!acme.poll_authorization(order.authorizations[auth]))
			mswait(1000);
		completed++;
	}
}
if (!completed)
	throw("No challenges fulfilled!");

/*
 * Wait for server to confirm
 */

/*
 * Create CSR
 * 
 * TODO: SANs, virtual hosts, etc...
 */
var csr = new CryptCert(CryptCert.TYPE.CERTREQUEST);

/*
 * Now open/create the keyset and RSA signing key for
 * Synchronet.
 * 
 * TODO: Regenerate keys etc.
 */
var opts = CryptKeyset.KEYOPT.NONE;
if (!file_exists(sks_fname))
	opts = CryptKeyset.KEYOPT.CREATE;
var sks = new CryptKeyset(sks_fname, opts);

var certrsa;
try {
	certrsa = sks.get_private_key("ssl_cert", syspass);
}
catch(e) {
	certrsa = new CryptContext(CryptContext.ALGO.RSA);
	certrsa.keysize=2048/8;
	certrsa.label="ssl_cert";
	certrsa.generate_key();
	sks.add_private_key(certrsa, syspass);
}
csr.subjectpublickeyinfo=certrsa;
// TODO: Read these from INI file?
csr.oganizationname=system.name;
csr.commonname=system.inet_addr;
csr.add_extension("2.5.29.17", false, create_dnsnames([system.inet_addr]));
csr.sign(certrsa);
csr.check();
var csrenc=csr.export(CryptCert.FORMAT.TEXT_CERTIFICATE);
order = acme.finalize_order(order, csr);

while (order.status !== 'valid') {
	order = acme.poll_order(order);
}

var cert = acme.get_cert(order);
cert.label = "ssl_cert";

/*
 * Delete the old certificate
 */
try {
	sks.delete_key(ssl_cert);
}
catch(e) {}
sks.add_public_key(cert);

/*
 * Recycle webserver
 */
file_touch(recycle_sem);

function older_than(days)
{
	if (!file_exists(sks_fname))
		return true;
	var sks = new CryptKeyset(sks_fname, CryptKeyset.KEYOPT.READONLY);
	try {
		var cert = sks.get_public_key("ssl_cert");
	}
	catch(e) {
		sks.close();
		return true;
	}
	sks.close();
	var now = new Date();
	var cutoff = cert.validfrom;
	cutoff.setDate(cutoff.getDate() + days);
	return now > cutoff;
}

function create_dnsnames(names) {
	var ext = '';
	var tmp;
	var count;

	function asn1_len(len) {
		var ret = '';

		if (len < 128)
			return ascii(len);
		var tmp = len;
		var count = 0;
		while (tmp) {
			ret = ascii(tmp & 0xff)+ret;
			tmp >>= 8;
			count++;
		}
		ret = ascii(0x80 | count) + ret;
		return ret;
	}

	for (var name in names) {
		ext = names[name] + ext;
		ext = asn1_len(names[name].length) + ext;
		ext = ascii(0x82) + ext;
	}
	ext = asn1_len(ext.length) + ext;
	ext = ascii(0x30) + ext;
	return ext;
}
